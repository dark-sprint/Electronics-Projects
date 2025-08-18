#include <dummy.h>

// Patricio Martinez - 2023.03.24
// Clase para gestionar configuraciones almacenadas en memoria interna (SPI flash)

#include "LittleFS.h"

class ConfigManager {

    public:
        char    ConfigFile[15] = "/config.txt"; // Archivo donde se almacenan las configuraciones
        String  rawConfig;             // Variable en memoria donde se almacenan la configuracion
        bool    fileSystem = false;    // Control de si se ha iniciado el sistema de ficheros
        bool    debug = false;         // Activar para trazar errores

        ConfigManager(){} // Constructor

        void InitFS(){
            Serial.print("Iniciando sistema de ficheros ...");
            if (!LittleFS.begin()){
                Serial.println(" [ERROR]");
                while (true) delay(1000);
            } else {
                Serial.println(" [OK]");
                fileSystem = true;
            }
        }

        void LoadSettings(){
            Serial.print("Leyendo configuración almacenada ... ");

            if ( !fileSystem ){
                Serial.println("[ERROR]\nEl sistema de ficheros no esta iniciado");
                return;
            }
            
            if ( !CargarEnMemoria() ){
                Serial.println("[ERROR]");
                Serial.println((String)"No se puede abrir el fichero: " + ConfigFile);
                // Generamos un fichero con opciones por defecto y reiniciamos
                ResetFactorySettings();
                // Reinciamos
                Serial.println("Reiniciando ...");
                delay(10);
                ESP.restart();
                
            } else {
                Serial.println("[OK]");
            } 
            
        }
        
        void ResetFactorySettings(){
            // Guardamos un archivo sin configuracion
            Serial.print((String)"[ConfigManager] Generando archivo " + ConfigFile + " con valores por defecto ... ");
            
            if ( !fileSystem ){
                Serial.println("[ERROR]\nEl sistema de ficheros no esta iniciado");
                return;
            }
            
            if ( GuardarStringDeConfiguracion("") ){
                Serial.println("[OK]");
            } else {
                Serial.println("[ERROR]");
            }
            
        }
        
        // Funcion para obtener un parametro de configuracion
        String Get(const String clave, const String defValor) {
            int inicio = 0, fin = 0;
            String resultado;
            inicio = rawConfig.indexOf(clave+"="); // Busco el inicio
            if ( inicio < 0 ){
                Save(clave, defValor);
                return defValor; // No se ha encontrado nada,
            }
            inicio = inicio + clave.length() + 1; // Avanzo la posicion de inicio
            fin = rawConfig.indexOf("\n", inicio); // Busco el final de la cadena
            if (fin == -1) fin = rawConfig.length(); // No hay salto de linea, es el ultimo valor
            resultado = rawConfig.substring(inicio, fin); // Capturamos valor
            if (debug) Serial.println((String)"[ConfigManager] GetValue("+clave+", "+defValor+") = "+resultado+" ("+inicio+","+fin+")");
            return resultado;
        }
        
        // Funcion para guardar o actualziar un parametro de configuracion
        bool Save(String clave, String valor) {
            Serial.println("[ConfigManager] Guardando "+clave+" => "+valor);
            int inicio = 0, fin = 0;
            
            // Buscamos si existe la clave para borrarla primero
            inicio = rawConfig.indexOf(clave+"=");    
            if ( inicio >= 0 ){
                fin = rawConfig.indexOf("\n", inicio); // Busco el final de la cadena
                if (fin == -1) fin = rawConfig.length(); // Hasta el final del string
                rawConfig.remove(inicio, fin-inicio+1); // Borramos la configuracion vieja
            }
            
            rawConfig = rawConfig+clave+"="+valor+"\n"; // Añadimos configuracion
            return GuardarStringDeConfiguracion(rawConfig);
        }
        
        
        void Import(String rawData){
            // Funcion para importar una configuracion y almacenarla en la memoria interna
            GuardarStringDeConfiguracion(rawData);
        }

        String Export(){
            // Funcion para exportar la configuracion en memoria
            return rawConfig;
        }
        
    private:
        
        bool CargarEnMemoria(){
            // Creo un recurso en modo lectura
            File rFile = LittleFS.open(ConfigFile, "r");
            
            // Si no puedo abrir el fichero, devuelvo error
            if (!rFile) {
                return false;
            }
            
            // Leo fichero y lo almaceno en una variable
            while (rFile.available()) {
              rawConfig += (char)rFile.read();
            }
            
            // Cierro el recurso
            rFile.close();
            if (debug) Serial.println("=== rawConfig ===\n"+rawConfig+"=================");
            return true;
        }

        bool GuardarStringDeConfiguracion(String strConfig){
            File file = LittleFS.open(ConfigFile, "w");
            // Si no puedo abrir el fichero, devuelvo error
            if (!file) return false; 
            // Guardamos contenido
            file.print(strConfig); 
            delay(10);
            file.close();
            if (debug) Serial.println("=== rawConfig ===\n"+rawConfig+"=================");
            return true;
        }
        
};

// Instancio la clase para poder usarla en cualquier momento
ConfigManager Settings;
