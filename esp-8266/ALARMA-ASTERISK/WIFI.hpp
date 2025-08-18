#include <ESP8266WiFi.h>

class WifiClass {
    public:
        String  Ssid;       // Nombre de red wireless
        String  Password;   // Password de red wifi
        String  Hostname;   // Nombre de equipo
        
        // Constructor
        WifiClass(){}

        void LoadSettings(){
            // Leemos la configuracion almacenada
            Ssid     = Settings.Get("WIFI_SSID", "GM-Produccion");
            Password = Settings.Get("WIFI_PASSWORD", "5P5JG8NYR79J6FD");
            Hostname = Settings.Get("WIFI_HOSTNAME", WiFi.hostname());
            WiFi.hostname(Hostname);
        }

        void IniciarModoCliente() {
            Serial.print((String) "Conectando a red WiFi "+ Ssid +" ");
            WiFi.mode(WIFI_STA);
            WiFi.begin(Ssid, Password);
            
            // Damos 10s para que se complete la conexion
            byte TimeOut = 0;
            while (WiFi.status() != WL_CONNECTED && TimeOut < 30 ){
                Serial.print(".");
                delay(500);
                TimeOut++;
            }
        
            if ( WiFi.status() == WL_CONNECTED ) {
                Serial.println(" [OK]");
                Serial.print(" - RSSI: ");
                Serial.println(WiFi.RSSI());
                Serial.print(" - IP address: ");
                Serial.println(WiFi.localIP());
                Serial.print(" - MAC Address: ");
                Serial.println(WiFi.macAddress());
                // Habilitamos modo persistente
                WiFi.setAutoReconnect(true);
                WiFi.persistent(true);
                
            } else {
                Serial.println(" [ERROR]");
                Serial.print(" - Status code: ");
                Serial.println(WiFi.status());
                WiFi.printDiag(Serial);
                while (1) delay(1000); // Stop!
            }
        
        }

        void IniciarModoAccessPoint(){    
            Serial.print((String) "Configurando WiFi en modo AP ("+Ssid+") ... ");
        
            // Configuro modo e inicio el modulo
            WiFi.mode(WIFI_AP);
            boolean result = WiFi.softAP(Ssid, Password);
            if(result == true) {
                Serial.println(" [OK]");
                Serial.print(" - IP address: ");
                Serial.println(WiFi.softAPIP());
            } else {
                Serial.println(" [ERROR]");
                while (1) delay(1000); // Paro la ejecucion
            }
        
        }

        int GetSignalPower() {
            int rssi = WiFi.RSSI();
            int power = 0;
            if (rssi <= -100) {
                power = 0;
            } else if(rssi >= -50) {
                power = 100;
            } else {   
                power = 2 * (rssi + 100);
            }
            //Serial.println((String)"[SignalPower] - RSSI: " + rssi + "dbm  PWR: " + power+"%");
            return power;
        }

};
// Instancio la clase para poder usarla 
WifiClass Wifi;

//funciones legacy
// Tarea para verificar el estado de la conexion
Ticker wifiTask;

/*-- NOTAS --------------------------------------------------------------------------
    WiFi.mode() soporta como parametro:
        WIFI_AP     = 0 // Modo access point
        WIFI_STA    = 1 // Modo station/cliente
        WIFI_AP_STA = 0 // Modo mixto o repetidor

    WiFi.status() puede devolver:
        WL_IDLE_STATUS      = 0,    // Wi-Fi está cambiando entre estados
        WL_NO_SSID_AVAIL    = 1,    // El SSID no se encuentra
        WL_SCAN_COMPLETED   = 2,    // Escaneo finalizado
        WL_CONNECTED        = 3,    // Conexion establecida
        WL_CONNECT_FAILED   = 4,    // Error de conexion, por ej. contraseña incorrecta
        WL_CONNECTION_LOST  = 5,    // Perdida de conexion
        WL_DISCONNECTED     = 6     // Está deconectado o no está en modo STA (station = cliente)
        WL_DISCONNECTED     = 7 
*/

// Monitor WiFi
void checkWifi() {
    // ESP.restart();
    if ( WiFi.status() != WL_CONNECTED ) {
        Serial.println("[MonitorWifi] Se ha perdido la conexion");
        // Print diagnostic information
        WiFi.printDiag(Serial); 
        
        switch (WiFi.status()){
          
          case WL_IDLE_STATUS:
            Serial.println("[MonitorWifi] Conexion en progreso (WL_IDLE_STATUS)");
            break;
          case WL_NO_SSID_AVAIL:
            Serial.println("[MonitorWifi] El SSID no es alcanzable (WL_NO_SSID_AVAIL)");
            break;
          case WL_CONNECT_FAILED:
            Serial.println("[MonitorWifi] Conexion fallida (WL_CONNECT_FAILED");
            break;
          case WL_DISCONNECTED:
            Serial.println("[MonitorWifi] Desconectado de la red (WL_DISCONNECTED)");
            break;
          case WL_CONNECTION_LOST:
            Serial.println("[MonitorWifi] Se ha perdido la conexion (WL_CONNECTION_LOST)");
            break;
        }

        Serial.print("[MonitorWifi] Intentando reconectar ...");
        WiFi.disconnect();
        WiFi.reconnect();
        Serial.println("[OK]");
        
        
    } else {
       Serial.println((String)"[MonitorWifi] Conexion correcta - Status code:"+WiFi.status()+" RSSI signal:"+WiFi.RSSI()); 
    }
}

void IniciarMonitorWifi(){
    Serial.print((String) "Habilitando monitor de conexión ...");
    wifiTask.attach(60, checkWifi);
    Serial.println("[OK]");
}

int getSignalPower() {
    int rssi = WiFi.RSSI();
    int power = 0;
    if (rssi <= -100) {
        power = 0;
    } else if(rssi >= -50) {
        power = 100;
    } else {   
        power = 2 * (rssi + 100);
    }
    //Serial.println((String)"[SignalPower] - RSSI: " + rssi + "dbm  PWR: " + power+"%");
    return power;
}
