# WIFI MQTT ENERGY SMARTMETER
 * PZEM : Read Power Consumption/Production
 * publish PZEM values on MQTT topic and MQTT Domoticz/in topic
 * display Power/Voltage on mini screen
 * Wired into Rail-DIN box

![photo](https://user-images.githubusercontent.com/53934994/136688865-a3b4bae1-0c27-487a-a898-0a9e817c8b39.png)


## Hardware requirements:

* PZEM Model : PZEM004T-100A v2.0, PZEM004T-100A v3.0   (v2.0 not supported)
   - PZEM-004T-v30         

* ESP Board : Wemos d1 mini (CH341 uart), esp8266
   - SoftSerial Method used on D5 D6 
   - When choosing GPIO pins to use, it's best to avoid GPIO 0, 2 and 15 (D3, D4, D8)

* Oled Shield 64x48 
   - I2C wired on D1-SCL D2-SDA

* AC-DC 5V 700mA-Small

* Plc RailDin Box ~8x37x59mm


## Wiring : 
        | Esp8266|...|PZEM004T-100A                     |    
        |--------|---|----------------------------------|
        | Vcc-5V |---|5V (1)  (violet)   ///      L-230V|
        | D5(TX) |->-|RX (2)  (vert)     ///      N-230V|      
        | D6(RX) |-<-|TX_(3)  (jaune)    ///        Coil|
        | GND    |---|GND (4) (bleu)     ///        Coil|
        
        *todo -> fritz schematic *

***
# BE CARREFULL 
# ON AC/dc CONNECTIONS
***

FYI : 
some Linux distrib (Ubuntu 20.x) failed on connect Uart CH340/1 while flashing ESP8266

     -> "Timed out waiting for packet header"
fixed in kernels 5.13.14 and maybe upper 
(https://cdn.kernel.org/pub/linux/kernel/v5.x/ChangeLog-5.13.14)
