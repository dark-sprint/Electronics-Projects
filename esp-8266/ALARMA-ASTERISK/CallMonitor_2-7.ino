// Version con pagina web para poder configurar parametros SIP
const char* APP_VERSION = "2.7";
const char* HW_VERSION = "1.01";
const char* BUILD_DATE = __DATE__ " " __TIME__;

// Configuracion de placa LC-Relay-ESP12-1R-MV
#define PIN_LED 16  // GPIO-16 (D0) Extra led
#define PIN_RELE 5  // GPIO-5 (D1)
#define PIN_SCL 12  // Por defecto es el GPIO-5 (D1), pero está en uso por el relé, uso el GPIO-14 (D6)
#define PIN_SDA 14  // Por defecto es el GPIO-4 (D2), me resulta más comodo/cercano usar el GPIO-12 (D5)

// Librerias y resto de proyecto
//#include <ESP8266WiFi.h>
#include <Ticker.h>
#include "ConfigManager.hpp"
#include "WiFi.hpp"
#include "UptimeClass.hpp"
#include "SipClass.hpp"
#include "AhtClass.hpp"
#include "WebServer.hpp"
#include "BlinkerLed.hpp"
#include "SnmpServer.hpp"

// Tareas
Ticker ahtTask; // Ticker para actualziar valores de los sensores
Ticker uptimeTask; // Ticker para monitorizar el uptime
Ticker snmpTask; // Ticker para atender las peticiones UDP del servicio SNMP

void CambioDeEstado(){
    Serial.println((String)"Nuevo estado: "+Sip.LastState);
    if (Sip.LastState == "early" ){
        ledBlinker.attach(0.15, blinkLed);  // Comienza parapadeo led
        digitalWrite(PIN_RELE, HIGH);       // Enciendo el rele
    } else {
        ledBlinker.detach(); // Paramos parpadeo
        digitalWrite(LED_BUILTIN, HIGH);    // Nos aseguramos de que queda apagado
        digitalWrite(PIN_RELE, LOW);        // Apago el rele
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000); // Delay para ver el arranque en el monitor al cargar sketchs
    Serial.println((String)"\nESP Core version: "+ESP.getCoreVersion());
    Serial.println((String)"SIP CallMonitor version: "+APP_VERSION+"\n");
    
    // Configura el pin del relé como salida
    pinMode(PIN_RELE, OUTPUT); 
    digitalWrite(PIN_RELE, LOW);   	

    // Inicializamos Led
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); // Durante el setup el led esta encencido, asi podemos saber que esta arrancando
    
    // SETTINGS
    Settings.InitFS();          // Iniciamos sistema de ficheros
    //Settings.ResetFactorySettings(); while (1) delay(1000); // Descomentar para desconfigurar
    Settings.LoadSettings();    // Cargamos archivo de configuracion en memoria
    
    // WIFI 
    Wifi.LoadSettings();        // Cargo configuracion almacenada
    Wifi.IniciarModoCliente();  // Nos conectamos a red wifi
    IniciarMonitorWifi();

    // WEBf
    Web.LoadSettings();     // Cargo configuracion almacenada
    IniciarWebServer();     // Iniciamos servidor web    

    // Iniciamos el servicio SNMP
    IniciarServicioSNMP();
    // Usamos una lambda para llamar al método de la instancia
    snmpTask.attach(0.1, []() { snmp.loop(); });
    
    // SIP
    Sip.LoadSettings();     // Cargo configuracion almacenada
    Sip.Init();             // Iniciamos escucha de peticiones UDP
    Sip.onChangeState(CambioDeEstado); // Asignamos una fucnion para los eventos de llamada

    // AHT21
    Aht.Init(); 
    // Usamos una lambda para llamar al método de la instancia
    ahtTask.attach(10, []() { Aht.Update(); });

    // Termina el setup, apagamos el LED
    digitalWrite(LED_BUILTIN, HIGH);

}

void loop() {
    // Atendemos respuestas del servidor
    Sip.Listener();
}
