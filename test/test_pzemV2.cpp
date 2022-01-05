#include <SoftwareSerial.h> // Arduino IDE <1.6.6
#include <Arduino.h>


#include "PZEM004Tv20.h"
// pzemRX bleu  D5 gpio14
// pzemTX blanc D6 gpio12
PZEM004T pzem(12,14);  // (RX,TX) connect to RX,TX of PZEM
IPAddress ip(192,168,1,1);

void setup() {
  Serial.begin(115200);
  pzem.setAddress(ip);
}

void loop() {
  float v = pzem.voltage(ip);
  if (v < 0.0) v = 0.0;
  Serial.print(v);Serial.print("V; ");

  float i = pzem.current(ip);
  if(i >= 0.0){ Serial.print(i);Serial.print("A; "); }
  
  float p = pzem.power(ip);
  if(p >= 0.0){ Serial.print(p);Serial.print("W; "); }
  
  float e = pzem.energy(ip);
  if(e >= 0.0){ Serial.print(e);Serial.print("Wh; "); }

  Serial.println();

//  delay(1000);
}
