/*
 * Clase para usar Aosong ASAIR AHT10 y AHT20
 * 2023.03.06 - Patricio Martinez
 * 
 */
 
#include <Wire.h>

class AhtClass {
    public:
        char  address = 0x38; // Default address
        float temperatura = .0;
        float humedad = 0;
        float rocio = 0;

        void Init(){           
            Serial.print("Iniciando sensor AHT10 ... ");
            //Wire.begin(address);
            Wire.begin(PIN_SDA, PIN_SCL, address);
            
            delay(20); // Wait to power on sensor
            
            while (!isConnected() & isBusy) {
                delay(10);
            }
        
            if ( (readStatus() & 0x68) == isCalibrated ) {
                Serial.println("[OK]");
            } else {
                Serial.println("[ERROR]");
            }
        }

        /* Esta funcion actualiza las lecturas almacenadas en la clase */
        void Update(){
            unsigned long result, temp[6];

            Wire.beginTransmission(address);
            Wire.write(cmdMeasure, 3);
            Wire.endTransmission();
            delay(100);
        
            // Leemos respuesta
            Wire.requestFrom(address, 6); 
            for(unsigned char i = 0; Wire.available() > 0; i++) {
                temp[i] = Wire.read();
            }   
        
            // Calculo temperatura (grados clesius)
            result = ((temp[3] & 0x0F) << 16) | (temp[4] << 8) | temp[5];
            temperatura = ((200 * result) / 1048576.0) - 50;
            
            // Calculo humedad relativa (porcentaje)
            result = ((temp[1] << 16) | (temp[2] << 8) | temp[3]) >> 4;
            humedad  = result * 100 / 1048576.0;

            // Calculo punto de rocio (grados celsius)
            rocio = (pow((humedad / 100),  0.125)) * (112 - (-(0.9 * temperatura))) - (-(0.1 * temperatura)) - 112; 
        }

        void reset(){
            Wire.beginTransmission(address);
            Wire.write(cmdReset);
            Wire.endTransmission();
            delay(20);
        }
        
    private:
        const char cmdCalibrate[3] = {0xE1, 0x08, 0x00};
        const char cmdMeasure[3] = {0xAC, 0x33, 0x00};
        const char cmdReset = 0xBA;
        const char isBusy = 0x80;
        const char isCalibrated = 0x08;
        
        unsigned char readStatus() {
            unsigned char result = 0;
            Wire.requestFrom(address, 1);
            result = Wire.read();
            return result;
        }

        bool isConnected() {
            Wire.beginTransmission(address);
            return (Wire.endTransmission() == 0);
        }
};

// Instancio la clase para poder usarla en cualquier momento
AhtClass Aht;
