// Mini clase para manejar configuraciones y funciones especificas
class WebClass {
    public:
        String  User;       // Usuario de gestion web
        String  Password;   // Password de gestion web
        int     RandomSeed; // Semilla aleatoria para generar los nonce
        
        // Constructor
        WebClass(){}

        void LoadSettings(){
            // Leemos la configuracion almacenada
            User     = Settings.Get("WEB_USER", "admin");
            Password = Settings.Get("WEB_PASSWORD", "esp8266");
            RandomSeed = random(0, 99999); 
        }
};
// Instancio la clase para poder usarla 
WebClass Web;

//#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Defino un objeto server de tipo AsyncWebServer de ambito global
AsyncWebServer server(80);

// Metodos de la API
void apiStatus(AsyncWebServerRequest *request){
    String json = "{";
           json+= "\"fw-version\":\"" + String(APP_VERSION) + "\",";
           json+= "\"fw-build-date\":\"" + String(BUILD_DATE) + "\",";
           
           // Hardware
           json+= "\"hw-version\":\"" + String(HW_VERSION) + "\",";
           json+= "\"hw-cpu-freq\":\"" + String(ESP.getCpuFreqMHz()) + "\",";
           json+= "\"hw-chip-id\":\"" + String(ESP.getChipId()) + "\",";
           json+= "\"hw-sdk\":\"" + String(ESP.getSdkVersion()) + "\",";
           json+= "\"hw-core\":\"" + String(ESP.getCoreVersion()) + "\",";
           json+= "\"hw-uptime\":\"" + String(Uptime.inSeconds()*1000) + "\",";
           json+= "\"hw-reset-reason\":\"" + String(ESP.getResetReason()) + "\",";
           json+= "\"hw-flash-size\":" + String(ESP.getFlashChipSize()) + ",";
           json+= "\"hw-sketch-size\":" + String(ESP.getSketchSize()) + ",";
           json+= "\"hw-sketch-free\":" + String(ESP.getFreeSketchSpace()) + ",";
           json+= "\"hw-stack-free\":" + String(ESP.getFreeContStack()) + ",";
           json+= "\"hw-heap-free\":" + String(ESP.getFreeHeap()) + ","; // tamaño de almacenamiento dinámico disponible ¿ram?
           json+= "\"hw-heap-frag\":" + String(ESP.getHeapFragmentation()) + ",";
           
           // WiFi
           json+= "\"wifi-hostname\":\"" + WiFi.hostname() + "\",";
           json+= "\"wifi-status\":\"" + String(WiFi.status()) + "\",";
           json+= "\"wifi-ssid\":\"" + WiFi.SSID() + "\",";
           json+= "\"wifi-bssid\":\"" + WiFi.BSSIDstr() + "\",";
           json+= "\"wifi-rssi\":\"" + String(WiFi.RSSI()) + "\",";
           json+= "\"wifi-signal\":\"" + String(Wifi.GetSignalPower()) + "\",";
           json+= "\"wifi-ip\":\"" + WiFi.localIP().toString() + "\",";
           json+= "\"wifi-nm\":\"" + WiFi.subnetMask().toString() + "\",";
           json+= "\"wifi-gw\":\"" + WiFi.gatewayIP().toString() + "\",";
           json+= "\"wifi-dns\":\"" + WiFi.dnsIP(0).toString() + "\",";
           json+= "\"wifi-mac\":\"" + String(WiFi.macAddress()) + "\",";

           // SIP
           json+= "\"sip-server\":\""+Sip.ServerIP+"\",";
           json+= "\"sip-server-port\":\""+String(Sip.ServerPort)+"\",";
           json+= "\"sip-caller-id\":\""+String(Sip.CallerID)+"\",";
           json+= "\"sip-user\":\""+Sip.User+"\",";
           json+= "\"sip-monitor-ext\":\""+Sip.MonitorExt+"\",";
           json+= "\"sip-last-state\":\""+Sip.LastState+"\",";
           json+= "\"sip-expire-session\":\""+String(Sip.ExpireSession)+"\",";
           
           // Sensores
           json+= "\"sensor-temperatura\":\""+String(Aht.temperatura)+"\",";
           json+= "\"sensor-humedad\":\""+String(Aht.humedad)+"\",";
           json+= "\"sensor-rocio\":\""+String(Aht.rocio)+"\"";
           
           json+= "}";
    request->send(200, "application/json", json);
}

void apiSensor(AsyncWebServerRequest *request){
    String json = "{";
           json+= "\"wifiSignal\":\""+String(WiFi.RSSI())+"\",";
           json+= "\"sensor-temperatura\":\""+String(Aht.temperatura)+"\",";
           json+= "\"sensor-humedad\":\""+String(Aht.humedad)+"\",";
           json+= "\"sensor-rocio\":\""+String(Aht.rocio)+"\"";
           json+= "}";
    request->send(200, "application/json", json);
}

void denyAccess(AsyncWebServerRequest *request){
    request->send(404);
}

String getNonce(String ClienteIP){
    String nonce = ClienteIP+Web.RandomSeed+CHIPID;
    nonce = sha1(nonce);
    return nonce;
}

bool checkAuth(String ClienteIP, String clientHash){
    // Calculo el nonce del cliente (unico)
    String nonce = getNonce(ClienteIP);
    // Calculo el hash que deberia haberme enviado
    String authHash = sha1(Web.User+":"+nonce+":"+Web.Password);
    // Comparo con lo que ha enviado
    if ( clientHash == authHash ){
        return true;
    } else {
        return false;
    }
}

void apiAuthNonce(AsyncWebServerRequest *request){
    // Generamos un nonce o hash unico para el usuario que lo solicita
    // El usuario nos devolvera el usuario y el password mezclado con este hash
    request->send(200, "text/plain", getNonce(request->client()->remoteIP().toString()));
}

void apiAuthToken(AsyncWebServerRequest *request){
    // Valido el acceso
    if ( checkAuth(request->client()->remoteIP().toString(), request->arg("Token") ) ){
        // Acceso valido
        request->send(200, "text/plain", "Acceso correcto");
        Serial.println("[Api/Auth/Token] Token valido");
    } else {
        request->send(401, "text/plain", "Usuario o password incorrecto");
        Serial.println("[Api/Auth/Token] Token invalido");
    }
}

void apiConfigRed(AsyncWebServerRequest *request){
    Serial.println("[Api/Config/Red] RemoteIP: " + request->client()->remoteIP().toString());
    
    // Auth required
    if (!checkAuth(request->client()->remoteIP().toString(), request->arg("Token") ) ){
        Serial.println("[Api/Config/Red] Token invalido");
        request->send(403, "text/plain", "Token invalido");
        return;
    }

    // Guardo configuraciones
    Settings.Save("WIFI_HOSTNAME", request->arg("wifi-hostname"));
    if ( request->arg("wifi-password") != "sin-cambios" ){
        Settings.Save("WIFI_SSID", request->arg("wifi-ssid"));
        Settings.Save("WIFI_PASSWORD", request->arg("wifi-password"));
    }

    // Cuando finalice la conexion debe reiniciar
    if ( request->arg("reboot").length() ){
        Serial.println("[Api/Config/Red] Solicitud de reiniciar aceptada");
        request->onDisconnect([](){
            ESP.restart();
        });
    }
    
    request->send(200, "text/plain", "Cambios guardados");  
}

void apiConfigSip(AsyncWebServerRequest *request){
    Serial.println("[Api/Config/Sip] RemoteIP: " + request->client()->remoteIP().toString());

    // Auth required
    if (!checkAuth(request->client()->remoteIP().toString(), request->arg("Token") ) ){
        Serial.println("[Api/Config/Sip] Token invalido");
        request->send(403, "text/plain", "Token invalido");
        return;
    }

    Settings.Save("SIP_SERVER", request->arg("sip-server"));
    Settings.Save("SIP_SERVER_PORT", request->arg("sip-server-port"));
    Settings.Save("SIP_CALLER_ID", request->arg("sip-caller-id"));
    Settings.Save("SIP_USER", request->arg("sip-user"));
    Settings.Save("SIP_PASSWORD", request->arg("sip-password"));
    Settings.Save("SIP_MONITOR_EXT", request->arg("sip-monitor-ext"));
    Settings.Save("SIP_EXPIRE_SESS", request->arg("sip-expire-session"));
    
    if ( request->arg("reboot").length() ){
        Serial.println("[Api/Config/Sip] Solicitud de reiniciar aceptada");
        request->onDisconnect([](){
            ESP.restart();
        });
    }
    
    request->send(200, "text/plain", "Cambios guardados");  
}

void apiReboot(AsyncWebServerRequest *request){
    Serial.println("[Api/Reboot] Token: "+request->arg("Token"));
    // Auth required
    if (!checkAuth(request->client()->remoteIP().toString(), request->arg("Token") ) ){
        Serial.println("[Api/Reboot] Token invalido");
        request->send(403, "text/plain", "Token invalido");
        return;
    }
    
    // Cuando finalice la conexion debe reiniciar
    request->onDisconnect([](){
        ESP.restart();
    });
    
    Serial.println("Reiniciando... ");
    request->send(200, "text/plain", "Reiniciando");
}


void apiUpdatePassword(AsyncWebServerRequest *request){
    // Auth required
    if (!checkAuth(request->client()->remoteIP().toString(), request->arg("Token") ) ){
        Serial.println("[Api/UpdatePassword] Token invalido");
        request->send(403, "text/plain", "Token invalido");
        return;
    }

    // Comprobamos password viejo
    if ( request->arg("Password") != Web.Password ){
        Serial.println("[Api/UpdatePassword] Password invalido");
        request->send(401, "text/plain", "Password invalido");
        return;
    }

    // Modificamos el password
    Web.Password = request->arg("newPassword");
    request->send(200, "text/plain", "Password actualizado");
    
    // Guardamos la configuracion
    Settings.Save("WEB_PASSWORD", Web.Password);
}

//--- Settings------------------------------------

void apiBackupSettings(AsyncWebServerRequest *request){
    // Auth required
    if (!checkAuth(request->client()->remoteIP().toString(), request->arg("Token") ) ){
        Serial.println("[Api/BackupSettings] Token invalido");
        request->send(403, "text/plain", "Token invalido");
        return;
    }

    // Cuando finalice la conexion debe reiniciar
    request->send(200, "text/plain", Settings.Export());
}

void apiUploadSettings(AsyncWebServerRequest *request){
    // Auth required
    if (!checkAuth(request->client()->remoteIP().toString(), request->arg("Token") ) ){
        Serial.println("[Api/RestoreSettings] Token invalido");
        request->send(403, "text/plain", "Token invalido");
        return;
    }

    // Cuando finalice la conexion debe reiniciar
    request->onDisconnect([](){
        ESP.restart();
    });

    String newSettings = request->arg("Settings");
    Serial.println((String)"[Api/RestoreSettings] Settings:" + newSettings);
    Settings.Import(newSettings);
    request->send(200, "text/plain", "Configuración guardada");
}

void apiRestoreSettings(AsyncWebServerRequest *request){
    // Auth required
    if (!checkAuth(request->client()->remoteIP().toString(), request->arg("Token") ) ){
        Serial.println("[Api/RestoreSettings] Token invalido");
        request->send(403, "text/plain", "Token invalido");
        return;
    }

    // Cuando finalice la conexion debe reiniciar
    request->onDisconnect([](){
        ESP.restart();
    });
    
    Settings.ResetFactorySettings();
    request->send(200, "text/plain", "Se ha restablecido la configuración");
}

void IniciarWebServer() {
    Serial.print("Levantando servidor web ...");
    // Protegemos el archivo donde se almacena la configuracion
    server.on("/config.txt", HTTP_GET, denyAccess);
    // Si el contenido existe en la memoria SPI, la cargo
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    // Declaramos funciones la api - GET
    server.on("/Api/Status", HTTP_GET, apiStatus);
    server.on("/Api/Sensor", HTTP_GET, apiSensor);
    server.on("/Api/Auth/Nonce", HTTP_GET, apiAuthNonce);
    // Declaramos funciones la api - POST
    server.on("/Api/Auth/Token", HTTP_POST, apiAuthToken);
    server.on("/Api/Config/Red", HTTP_POST, apiConfigRed);
    server.on("/Api/Config/Sip", HTTP_POST, apiConfigSip);
    server.on("/Api/Reboot", HTTP_POST, apiReboot);
    server.on("/Api/UpdatePassword", HTTP_POST, apiUpdatePassword);
    server.on("/Api/BackupSettings", HTTP_POST, apiBackupSettings);
    server.on("/Api/UploadSettings", HTTP_POST, apiUploadSettings);
    server.on("/Api/RestoreSettings", HTTP_POST, apiRestoreSettings);
    // El resto, no esta soportado
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404);
    });

    // Para poder cargar los datos json desde otro servidor web
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    server.begin();
    Serial.println(" [OK]"); 
    Serial.println(" - Usuario: "+Web.User+" ("+Web.User.length()+")\n - Password: "+Web.Password+" ("+Web.Password.length()+")");
}
