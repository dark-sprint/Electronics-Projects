// Parpadeo led
#include <Ticker.h>

Ticker ledBlinker;

void blinkLed() {
    //Serial.println("Blink!");
    int state = digitalRead(LED_BUILTIN);  
    digitalWrite(LED_BUILTIN, !state);     
}
