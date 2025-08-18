class UptimeClass {
    public:

        void Update(){
            // Solo debemos controlar el numero de loops que ha realizado millis
            if (millis() < ultimaLectura) {
                loops++;
            }
            ultimaLectura = millis();
        }

        String Get(bool FormatoLargo = false){
            unsigned long TS = millis(); // Tiempo en ms de esta vuelta
            unsigned int d = 0; // Total de dias uptime
            unsigned int h = 0; // Total de horas uptime
            unsigned int m = 0; // Total de minutos uptime
            unsigned int s = 0; // Total de segundos uptime

            // Si la funcion millis() ha dado alguna vuelta, calculamos tiempo añadido
            if ( loops > 0 ) {
                d = (loops * 49);
                h = (loops * 17);
                m = (loops * 2);
                s = (loops * 47);
            }

            // Ahora solo falta calcular cuanto tiempo hay en la ultima vuelta
            d += int(TS / (1000*60*60*24));
            h += int((TS / (1000*60*60)) % 24);
            m += int((TS / (1000*60)) % 60);
            s += int((TS / (1000)) % 60);

            // Y por ultimo devolvemos el string            
            char buffer[50];
            if (FormatoLargo) {
                sprintf(buffer, "%d dias, %02d horas, %02d minutos, %02d segundos", d, h, m, s);
            } else {
                sprintf(buffer, "%d days %02d:%02d:%02d", d, h, m, s);
            }
            
            return (String)buffer; 
        }

        uint32_t inSeconds(){
            // Con un long unsigned tengo un maximo de 4,294,967,295 que son mas de 136 años
            unsigned long segundos = 0;

            if ( loops > 0 ) {
                segundos = (loops * 4294967);
            }

            // Añadimos los segundos que han pasado en el bucle actual
            segundos += long(millis()/1000);

            return segundos;
        }
            
    private:
        unsigned int ultimaLectura; // Timestamp - Es una copia de la ultima lectura, y la guardo para saber si se ha desbordado millis()
        unsigned int loops = 0; // Contador de veces que millis ha dado la vuelta

};

// Instancio la clase para poder usarla en cualquier momento
UptimeClass Uptime;

// Una funcion para poder actualizar el uptime desde un ticker
void uptimeUpdate() {
    Uptime.Update();
    //Serial.println((String) "[Uptime] "+Uptime.Get());
}
