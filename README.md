# WIFI MQTT ENERGY SMARTMETER
 * **PZEM : Read Power** _Consumption (OR Production)_ - sample rate : 2s
 * Data sent to your **Domoticz** Box, raspberry, PC... using a local MQTT Broker
 
            -> data are synchronized on PZEM sample rate or DomoticzPubTimer (let see AP Webserver)
 
 * Publish **power** and **voltage** on Domotics/MQTT → _'domoticz/in'_ topic
 * Publish **all** PZEM **values** on MQTT → _'Pzem topic'_
 * Display Power/Voltage/Hz on mini screen   
 * Accuracy : ~1 % (compared to ENEDIS/Linky Webservice) - measure using Integral on Grafana (instead of Domoticz reports)
 * Wired inside Rail-DIN box

![photo](https://user-images.githubusercontent.com/53934994/136688865-a3b4bae1-0c27-487a-a898-0a9e817c8b39.png)
![photo](https://user-images.githubusercontent.com/53934994/137083496-70fa6ab4-3972-4f08-b075-35438a764d2d.png)

 * Wifi Access Point **WebServer** and set **custom parameters**
 * WebOTA : On The Air firmware update
        
        URL : http://<pzem_ip>:8080/update
        
        <pzem_ip> : this ip is published in the bootstrap Mqtt message ( topic : _pzem_topic_)

![photo](https://user-images.githubusercontent.com/53934994/139536819-df299a4f-86d1-45ee-afe6-58e61d8bed9b.png)

## Hardware requirements:   ~15 €

* PZEM Model : PZEM004T-100A v3.0   (or old v2.0)
   - PZEM-004T-v30         

* ESP Board : Wemos d1 mini (CH341 uart), esp8266
   - SoftSerial Method used on D5 D6 
   - When choosing GPIO pins to use, it's best to avoid GPIO 0, 2 and 15 (D3, D4, D8)

* Oled Shield 64x48 
   - I2C wired on D1-SCL D2-SDA

* AC-DC 5V 700mA-Small

* Plc RailDin Box ~8x37x59mm


## Wiring : 
  | Esp8266|       |PZEM004T-100A   |           |    
  |--------|-------|----------------|-----------|
  | Vcc-5V |-------|5V  (1)         |    230v-N | 
  | D5(TX) |--->---|RX  (2)         |    230v-L |    
  | D6(RX) |---<---|TX  (3)         |      Coil | 
  | GND    |-------|GND (4)         |      Coil | 
                                                                                                                               
  
![photo](https://user-images.githubusercontent.com/53934994/139558602-1654c534-b2dc-4c6d-933c-fd18c7fac8af.png)

# BE CARREFULL 
# ON AC/dc CONNECTIONS

## Firmware

→ https://github.com/Coturex/Wifi_Mqtt_SmartMeter/tree/main/Releases

## Domoticz settings

In domoticz you will have to activate the MQTT protocol and install (mosquitto) MQTT server on your home automation box. (raspberry or PC ...).
The power measurement (W) must then be associated with a virtual sensor
   - Type of sensor: Electric (instantaneous + counter)
   - Energy Read: * computed *
   - →  Let remember the idx of this new device, it will be informed trougth the Access Point Wifi Manager
   
## Tips

Here are different 'On_message' MQTT commands :

 Topic : 

    pzem_topic / pzem_id / cmd

|Command | Action                                     |
|--------|--------------------------------------------|
| bs     | will send an Mqtt bootstrap message        |
| reboot | will reboot the device                     |
| ap     | will start AccessPoint Settings            |
| reset  | will reset Energy Counter _(only PZEM v3)_ |
 
 
 


## FYI  
some Linux distrib (Ubuntu 20.x) failed on connect Uart CH340/1 while flashing ESP8266

     →  "Timed out waiting for packet header"
fixed in kernels 5.13.14 and maybe upper 
(https://cdn.kernel.org/pub/linux/kernel/v5.x/ChangeLog-5.13.14)

## Todo :

