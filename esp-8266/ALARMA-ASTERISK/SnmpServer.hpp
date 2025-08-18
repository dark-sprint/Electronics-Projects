#include <WiFiUdp.h>
#include <SNMP_Agent.h>

// Configuracion SNMP
const char* SNMP_COMMUNITY = "public";

// Declaramos funciones para evitar que el compilador falle porque no las encuntra
void IniciarServicioSNMP();
void MIB_RFC1213();
void MIB_Custom();
uint32_t snmpGetUptime();
int snmpGetRssi();
int snmpGetTemperatura();
int snmpGetHumedad();
int snmpGetRocio();

WiFiUDP udp; // Objeto UDP
// Inicia una instancia SMMPAgent con la community de sólo lectura
// No establecemos la community de lectura-escritura
SNMPAgent snmp = SNMPAgent(SNMP_COMMUNITY);  

void IniciarServicioSNMP() {

    Serial.print("Iniciando agente SNMP ... ");

    snmp.setUDP(&udp);
    snmp.begin();

    MIB_RFC1213();
    MIB_Custom();
    Serial.println("[OK]");
}

void MIB_RFC1213(){
    // Add SNMP Handlers of correct type to each OID
    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.1.1.0", std::string("SensorRS ") + APP_VERSION); // sysDescr
    snmp.addOIDHandler(".1.3.6.1.2.1.1.2.0", ".1.3.6.1.4.1.8266.483"); // sysObjectID (Donde estan mi propio mib)
    snmp.addDynamicReadOnlyTimestampHandler(".1.3.6.1.2.1.1.3.0", &snmpGetUptime); // sysUptime
    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.1.4.0", "Patricio Martinez <gpatricio@gmail.com>"); // sysContact
    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.1.5.0", WiFi.hostname().c_str()); // sysName
    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.2.1.1.6.0", "Grafometal S.A."); // sysLocation
    snmp.addReadOnlyIntegerHandler(".1.3.6.1.2.1.1.7.0", 65); // sysServices
}

void MIB_Custom(){
    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.4.1.8266.483.0.1", "WiFi Signal Power"); 
    //snmp.addIntegerHandler(".1.3.6.1.4.1.8266.483.0.2", &WifiRssi); 
    snmp.addDynamicIntegerHandler(".1.3.6.1.4.1.8266.483.0.2", &getSignalPower); // Funcion en WiFi.hpp
    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.4.1.8266.483.0.3", "%");  
    snmp.addReadOnlyIntegerHandler(".1.3.6.1.4.1.8266.483.0.4", 1); // Escala (divisor)

    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.4.1.8266.483.1.1", "Temperatura"); 
    snmp.addDynamicIntegerHandler(".1.3.6.1.4.1.8266.483.1.2", &snmpGetTemperatura); 
    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.4.1.8266.483.1.3", "*C");
    snmp.addReadOnlyIntegerHandler(".1.3.6.1.4.1.8266.483.1.4", 100); // Escala (divisor)

    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.4.1.8266.483.2.1", "Humedad"); 
    snmp.addDynamicIntegerHandler(".1.3.6.1.4.1.8266.483.2.2", &snmpGetHumedad); 
    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.4.1.8266.483.2.3", "%");
    snmp.addReadOnlyIntegerHandler(".1.3.6.1.4.1.8266.483.2.4", 100); // Escala (divisor)

    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.4.1.8266.483.3.1", "Rocio"); 
    snmp.addDynamicIntegerHandler(".1.3.6.1.4.1.8266.483.3.2", &snmpGetRocio); 
    snmp.addReadOnlyStaticStringHandler(".1.3.6.1.4.1.8266.483.3.3", "*C");
    snmp.addReadOnlyIntegerHandler(".1.3.6.1.4.1.8266.483.3.4", 100); // Escala (divisor)
}

// Funciones para acceder a valores mediante punteros
uint32_t snmpGetUptime() {
    // La libreria SNMP ahora soporta punteros a funciones, pero no a se puede acceder directamente a metodos de clases, asi que envolvemos la llamada en una funcion
    // Debo convertir el uptime, porque en SNMP se expresa en cientos de segundos:
    return Uptime.inSeconds()*100;
}

int snmpGetTemperatura() {
    // SNMP no soporta float, para dos decimales multiplico por 100 y convierto a entero
    return int(Aht.temperatura*100);
}

int snmpGetHumedad() {
    return int(Aht.humedad*100);
}

int snmpGetRocio() {
    return int(Aht.rocio*100);
}
