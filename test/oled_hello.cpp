#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_SSD1306.h"

 
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);
 

 
void setup()   
{
  Serial.begin(115200);
  
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.display();
}
 
void loop() 
{
   // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  //display.display();
  delay(2000);

  // // Clear the buffer.
  display.clearDisplay();
  
  // positionne le curseur dans le coin supérieur gauche - clear display and set cursor on the top left corner
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("HELLO");
  Serial.print("hello - "); 
  display.display();
  
  // // Température en Celcius - Temperature in Celcius
    
}