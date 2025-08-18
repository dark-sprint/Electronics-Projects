// Patricio Martinez - 2023.03.24
// Clase para conectarse a un servidor SIP (como Asterisk) y suscribirse al estado de una extension.
// Adicionalmente se pueden hacer y recibir llamadas con la extension que se loguea en el servidor.
// Se ha rescrito la completo y se han implementado muchas funciones que la clase original no tenia.

#include <WiFiUdp.h>

class SipClass {
    public:
        String  ServerIP;           // IP del servidor SIP
        int     ServerPort;         // Puerto del servidor
        String  ClientIP;           // IP del cliente, se configura cuando se completa la conexion WiFi
        int     ClientPort;         // Puerto donde escucha el cliente
        String  User;               // SIP Username
        String  Password;           // SIP Password
        String  CallerID;           // SIP CallerID, el nombre que aparecera si realiza llamadas
        String  MonitorExt;         // Extension a monitorizar
        int     ExpireSession;      // Cada cuanto tiempo debe renovarse la suscripcion
        String  LastState;          // Ultimo estado de la extension
        bool    debug = true;      // Establecer en true para ver los paquetes por consola
        
        SipClass(){} // Constructor
        
        void LoadSettings(){
            ServerIP      = Settings.Get("SIP_SERVER", "10.1.70.201");
            ServerPort    = Settings.Get("SIP_SERVER_PORT", "5060").toInt();
            ClientIP      = WiFi.localIP().toString();
            ClientPort    = 5072;
            User          = Settings.Get("SIP_USER", "2500");
            Password      = Settings.Get("SIP_PASSWORD", "pw2500");
            CallerID      = Settings.Get("SIP_CALLER_ID", "SipCallMonitor");
            MonitorExt    = Settings.Get("SIP_MONITOR_EXT", "2036");
            ExpireSession = Settings.Get("SIP_EXPIRE_SESS", "1800").toInt();
        }

        void Init(){
            Serial.print("Levantando servicio SIP ...");

            if ( Udp.begin(ClientPort) ){
                Serial.println(" [OK]");
                Serial.println((String)"[SIP] Escuchando conexiones UDP en puerto " + ClientPort);
            } else {
                Serial.println(" [ERROR]");
                while (true) delay(1000);
            }

            // Iniciamos la suscripcion a la extension a monitorizar
            Subscribe();
        }

        // Funcion a ejecutar dentro del loop principal para procesar los paquetes entrantes
        void Listener(){
            int packetSize = Udp.parsePacket(); // Devuelve el tamaño total del paquete vamos a recibir
            if (packetSize > 0) {
                int len = Udp.read(pBufIn, packetSize); // Leemos el paquete UDP y lo cargamos dentro de pBufIn 
                if (len > 0){
                    pBufIn[len] = '\0'; // Para finalizar un String, debemos añadir un caracter ASCII 
                    if (debug){
                        Serial.println();
                        Serial.println((String)"<< From: " + Udp.remoteIP().toString() + ":" + Udp.remotePort()+" (" + packetSize + " bytes)");
                        Serial.println("-----------------------------------------------------------");
                        Serial.println(pBufIn);
                        Serial.println("-----------------------------------------------------------");
                    }
                    ResponseProcessor();
                }
            }
            CheckSession();
        }

        // Funcion para declarar que se debe ejecutar como Callback
        void onChangeState( void( * functionCallback)()) {
            // Como parametro recoge el puntero de donde se encuentra almacenada la funcion fuera de esta clase
            // El puntero lo llamamos functionCallback
            eventChangeState = functionCallback;
        }

    private:
        WiFiUDP   Udp; // Objeto para manejar conexiones UDP
        char      pBufIn[2048];  // Buffer para almacenar los mensajes que nos envia el servidor
        char      pBufOut[2048]; // Buffer para almacenar los mensajes que enviamos al servidor
        int       cSeq; // Contador de secuencia. Es parte del paquete UDP. Se debe incrementar el valor de CSeq por cada llamada para una misma CallID.
        uint32_t  callId;
        uint32_t  tagId;
        uint32_t  branchId;
        uint32_t  lastSubscribe = 0; // Manejo una marca de tiempo para saber cuando ha sido la ultima suscripcion
        
        
        void ResponseProcessor(){
            // Se requiere autenticacion
            if (strstr(pBufIn, "SIP/2.0 401 Unauthorized")) {
                if (strstr(pBufIn, "SUBSCRIBE")) {
                    // Repetimos SUBSCRIBE usando Digest Realm
                    Subscribe(pBufIn);
                    return;
                }
                
                Serial.println("[SIP] Unauthorized - No hay metodo disponible");
                return;
            }

            // Registro correcto
            if (strstr(pBufIn, "SIP/2.0 200 OK")) {
                if (strstr(pBufIn, "REGISTER")) {
                    Serial.println("[SIP] REGISTER - Registro correcto");
                    lastSubscribe = millis(); // Actualizo registro
                    return;
                }
                
                Serial.println("[SIP] No hay metodo disponible");
                return;
            }

            if (strstr(pBufIn, "NOTIFY")) {
                // El servidor esta enviando alguna notificacion, debemos aceptarla con "SIP/2.0 200 OK"
                AcceptNotify();
                return;
            }
            
        }

        void Subscribe(const char * paquete = 0){
            
            // Control de secuencia y prevencion de loops
            if (!paquete) {
                // Es una solcitud nueva, asignamos valores
                Serial.println("[SIP] Iniciamos SUBSCRIBE para extension "+MonitorExt);
                cSeq = 0;
                callId = random(0xFFFFFFFF);
                tagId = random(0xFFFFFFFF);
                branchId = random(0xFFFFFFFF);
                
            } else {
                Serial.println("[SIP] Repetimos SUBSCRIBE usando Digest Realm");           
                cSeq++;
                if ( cSeq > 3) {
                    Serial.println("[SIP] ERROR - Bucle detenido en SUBSCRIBE");
                    return;
                }
            }

            // Construimo un paquete de respuesta nuevo
            pBufOut[0] = 0;  
            AddSipLine("SUBSCRIBE sip:%s@%s SIP/2.0", MonitorExt, ServerIP.c_str());
            AddSipLine("Via: SIP/2.0/UDP %s:%i;branch=%010u;rport=%i", ClientIP.c_str(), ClientPort, branchId, ClientPort);
            AddSipLine("From: \"%s\" <sip:%s@%s>;tag=%010u", CallerID.c_str(), User, ServerIP.c_str(), tagId);
            AddSipLine("To: <sip:%s@%s>", MonitorExt.c_str(), ServerIP.c_str());
            AddSipLine("Call-ID: %010u@%s", callId, ClientIP.c_str());
            AddSipLine("CSeq: %i SUBSCRIBE", cSeq);
            AddSipLine("Contact: \"%s\" <sip:%s@%s:%i;transport=udp>", User.c_str(), User.c_str(), ClientIP.c_str(), ClientPort);
            if (paquete){
                AddAuthLine(); // Si hay datos de Digest, podré añadir los datos de autenticacion
            }
            AddSipLine("Max-Forwards: 70");
            AddSipLine("User-Agent: SipCallMonitor v%s", APP_VERSION);
            AddSipLine("Expires: %d", ExpireSession);
            AddSipLine("Event: dialog");
            AddSipLine("Accept: application/dialog-info+xml");
            AddSipLine("Content-Length: 0");
            AddSipLine("");
            SendUdp();
            
        }

        void CheckSession(){
            // Esta funcion se ejecuta continuamente y comprueba si la sesion ha caducado
            if ( (millis() - lastSubscribe) > ExpireSession*1000 ){
                Serial.println("[SIP] La session ha expirado, volvemos a realizar un SUBSCRIBE");
                lastSubscribe = millis();
                Subscribe();
            }
        }

        void AcceptNotify(){
            Serial.println("[SIP] Aceptamos NOTIFY del servidor");
            SendOk();

            // Procesamos el notify para saber que esta sucediendo
            // Si hay un puntero para EventChangeState, ejecutamos la funcion
            if (eventChangeState) {
                LastState = GetValue("<state>", "<", pBufIn);
                ( * eventChangeState)();
            }
        }
        
        // Defino como debe ejecutarse la funcion mapeada
        void (*eventChangeState)(); 
        
        
        void SendOk() {
            pBufOut[0] = 0;
            AddSipLine("SIP/2.0 200 OK");
            AddCopySipLine("Call-ID: ");
            AddCopySipLine("CSeq: ");
            AddCopySipLine("From: ");
            AddCopySipLine("Via: ");
            AddCopySipLine("To: ");
            AddSipLine("Content-Length: 0");
            AddSipLine("");
            SendUdp();
        }
        
        // Funcion que intenta obtener un valor de una cadena 
        String GetValue(const char * inicio, const char * fin, const char * datos) {
            // Despues de muchas, muchas horas, he desestimado devolver char* porque realmente devuelve punteros de memoria
            // Cada vez que se ejecute la funcion, se reescribira en el espacio de memoria y todos los valores asignados (punteros) veran otro valor...
            // Para localizar el valor que busco, uso punteros de memoria, es muy rapido
            String resultado;
            const char * p1; // Puntero de inicio
            const char * p2; // Puntero de fin
            
            if ((p1 = strstr(datos, inicio)) != NULL) {
                p1 = p1 + strlen(inicio); // Avanzo el puntero a donde empieza el valor
                p2 = strstr(p1, fin); // Busco el puntero donde termina el valor
                int len = p2 - p1; // Calculo la longitud de la cadena obtenida
                char pRes[len]; // Creamos un puntero para guardar el resultado
                strncpy(pRes, p1, len); // Copio los punteros de memoria en pRes
                pRes[len] = '\0';
                resultado = pRes; // Asigno al string el valor del puntero
            }
            //Serial.println((String)"[resultado]: "+resultado);
            return resultado;
        }
        
        void AddSipLine(const char * linea, ...) {
            va_list arglist;
            va_start(arglist, linea);
            uint16_t len = (uint16_t) strlen(pBufOut);
            char * p = pBufOut + len;
            vsnprintf(p, 2048-len, linea, arglist);
            va_end(arglist);
            len = (uint16_t) strlen(pBufOut);
            if (len < (2048 - 2)) {
                pBufOut[len] = '\r';
                pBufOut[len + 1] = '\n';
                pBufOut[len + 2] = '\0';
            }
        }

        void AddCopySipLine(const char * buscar) {
            char * p1; // Puntero de inicio
            if ( p1 = strstr(pBufIn, buscar) ) {
                // Creo punero final en fin de linea
                char * p2 = strstr(p1, "\r");
                if (p2 == 0) p2 = strstr(p1, "\n");
                // Si pinta bien, copio la linea
                if (p2 > p1) {
                    char c = * p2;
                    * p2 = 0;
                    AddSipLine("%s", p1);
                    * p2 = c;
                }
            }
        }

        void AddAuthLine(){
            // WWW-Authenticate: Digest realm="asterisk",nonce="1681468905/efe79f2e30e3366706acdb9b6eff2bd0",opaque="208f6ee93cab5bc3",algorithm=MD5,qop="auth"
            
            // Si puedo obtener el realm y el nonce, calculo el hash de autenticacion y añado la linea al paquete
            String realm = GetValue("realm=\"", "\"", pBufIn);
            String nonce = GetValue("nonce=\"", "\"", pBufIn);

            if ( realm.length()  && nonce.length() ) {
                
                if (debug) Serial.println((String)"[SIP] Digest - Usando realm:"+realm+" nonce:"+nonce);
                // HA1 = MD5(username:realm:password)
                // HA2 = MD5(method:digestURI)
                // response = MD5(HA1:nonce:HA2)
                String ha1 = (String)User+":"+realm+":"+Password;
                String ha2 = (String)"SUBSCRIBE:sip:"+MonitorExt+"@"+ServerIP;
                String response = md5(md5(ha1)+":"+nonce+":"+md5(ha2));
                if (debug) Serial.println((String)"[SIP] Digest - ha1:"+ha1+" ha2:"+ha2+" response:"+response);
                
                // Añadimos la linea calculada a la respuesta
                AddSipLine("Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"sip:%s@%s\", response=\"%s\", algorithm=MD5", User.c_str(), realm.c_str(), nonce.c_str(), MonitorExt.c_str(), ServerIP.c_str(), response.c_str());
            }
        }
        
        String md5(String str) {
            MD5Builder _md5;
            _md5.begin();
            _md5.add(String(str));
            _md5.calculate();
            return _md5.toString();
        }
        
        int SendUdp() {
            Udp.beginPacket(ServerIP.c_str(), ServerPort);
            Udp.write(pBufOut, strlen(pBufOut));
            Udp.endPacket();

            if (debug){
                Serial.println();
                Serial.println((String)">> To: " + ServerIP + ":" + ServerPort+" (" + strlen(pBufOut) + " bytes)");
                Serial.println("-----------------------------------------------------------");
                Serial.println(pBufOut);
                Serial.println("-----------------------------------------------------------");
            }
            return 0;
        } 

};

// Instancio la clase para poder usarla en cualquier momento
SipClass Sip;
