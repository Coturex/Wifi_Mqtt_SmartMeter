#include <Arduino.h>
#include <SoftwareSerial.h>

#include "PZEM004Tv10.h"

#if defined(ESP32)
    #error "Software Serial is not supported on the ESP32"
#endif

PZEM004Tv10 pzem(D5,D6);

void setup() {
    /* Debugging serial */
    Serial.begin(115200);
    while(!pzem.init()) {
        Serial.print(".") ;
        delay(1000);
    }
}

void loop() {
         
    Serial.print("pzem v1 :");
    
    // Read the data from the sensor
    float voltage = pzem.voltage();
    float current = pzem.current();
    float power = pzem.power();
    float energy = pzem.energy();
    
    // Check if the data is valid
    if(isnan(voltage)){
        Serial.println("Error reading voltage");
    } else if (isnan(current)) {
        Serial.println("Error reading current");
    } else if (isnan(power)) {
        Serial.println("Error reading power");
    } else if (isnan(energy)) {
        Serial.println("Error reading energy");
    }  else {

        // Print the values to the Serial console
        if(voltage != -1 && current != -1 && power != 0xffff && energy != 0xffffffff) {
            Serial.print("Voltage: ");      Serial.print(voltage);      Serial.println("V");
            Serial.print("Current: ");      Serial.print(current);      Serial.println("A");
            Serial.print("Power: ");        Serial.print(power);        Serial.println("W");
            Serial.print("Energy: ");       Serial.print(energy);       Serial.println("kWh");
        }
    }

    Serial.println();
    delay(3000); 
}
