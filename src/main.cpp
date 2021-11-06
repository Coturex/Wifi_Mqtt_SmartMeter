/*
 * Copyright (C) 2021-2022 Coturex - F5RQG
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Wemos pin use : it's best to avoid GPIO 0, 2 and 15 (D3, D4, D8)
// D5 : RX pzem
// D6 : TX pzem
// D1 : I2C clock - OLED
// D2 : I2C data  - OLED

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>       // Mqtt lib
#include <SoftwareSerial.h>     // ESP8266/wemos requirement
#include <WiFiManager.h>        // Manage Wifi Access Point if wifi connect failure (todo : and mqtt failure)
#include "PZEM004Tv30.h"        // comment if not used
//#include "PZEM004Tv20.h"      // comment if not used
#include "Adafruit_SSD1306.h"   // OLED requirement

#define USEOTA // enable On The Air firmware flash 
#ifdef USEOTA
#include "WebOTA.h"
#endif

//#include "myConfig_sample.h"  // Personnal settings - 'gited file'
//#include "myConfig.h"           // Personnal settings - Not 'gited file'
#include <EEPROM.h>             // EEPROM access...


#define FW_VERSION "1.1"
#define DOMO_TOPIC "domoticz/in" 

// Wait this duration between each measurement (milliseconds). This is added to the time needed to read data (~2s)
#define PERIOD_V20 3000
#define PERIOD_V30 2000

// I2C OLED screen stuff
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET); // Wemos I2C : D1-SCL D2-SDA

// WiFi + MQTT stuff.
WiFiClient espClient ;
PubSubClient mqtt_client(espClient);  
String cmdTopic;
String outTopic;
#define MQTT_RETRY 5     // How many retries before starting AccessPoint

// Setup our pzem module with these pins : pins must be chosen carefully so that the ESP can/cannot boot.
// Wemos : it's best to avoid GPIO 0, 2 and 15 (D3, D4, D8)
// Wemos must use softwareSerial method
SoftwareSerial pzemSWSerial1(D5, D6);
PZEM004Tv30 pzem1 ; // (RX,TX)connect to TX,RX of PZEM

//  TEST & DEBUG OPTION FLAGS
bool DEBUG = true ;
bool TEST_CP         = false; // AP : always start the ConfigPortal, even if ap found
int  TESP_CP_TIMEOUT = 30;    // AP : AccessPoint timeout and leave AP
bool ALLOWONDEMAND   = true;  // AP : enable on demand - e.g Trigged on Gpio Input, On Mqtt Msg etc...

#define MAX_STRING_LENGTH 35         
struct { 
    char name[MAX_STRING_LENGTH] = "";
    char mqtt_server[MAX_STRING_LENGTH] = "";
    char mqtt_port[MAX_STRING_LENGTH] = "";
    char pzem_topic[MAX_STRING_LENGTH] = "";
    char pzem_id[MAX_STRING_LENGTH] = "";
    char idx_power[MAX_STRING_LENGTH] = "";
    char idx_voltage[MAX_STRING_LENGTH] = "";
    bool AP = 0 ;
  } settings;

WiFiManager wm;
WiFiManagerParameter custom_name("name", "Oled Title", "", 15);
WiFiManagerParameter custom_mqtt_server("mqtt_server", "mqtt IP server", "", 15);
WiFiManagerParameter custom_mqtt_port("mqtt_port", "mqtt Port", "", 4);
WiFiManagerParameter custom_pzem_topic("pzem_topic", "Pzem Topic", "", 35);
WiFiManagerParameter custom_pzem_id("pzem_id", "Pzem ID", "", 15);
WiFiManagerParameter custom_idx_power("idx_power", "Domoticz idx power", "", 4); 
WiFiManagerParameter custom_idx_voltage("idx_volt", "Domoticz idx voltage", "", 4);

void oled_cls(int size) {
    // OLED : set cursor on top left corner and clear
    display.clearDisplay();
    display.setTextSize(size);
    display.setTextColor(WHITE); // seems only WHITE exist on this oled model :(
    display.setCursor(0,0);
}

void saveWifiCallback() { // Save settings to EEPROM
    unsigned int addr=0 ;
    Serial.println("[CALLBACK] saveParamCallback fired"); 
    strncpy(settings.name, custom_name.getValue(), MAX_STRING_LENGTH);  
    strncpy(settings.mqtt_server, custom_mqtt_server.getValue(), MAX_STRING_LENGTH);  
    strncpy(settings.mqtt_port, custom_mqtt_port.getValue(), MAX_STRING_LENGTH);  
    strncpy(settings.pzem_topic, custom_pzem_topic.getValue(), MAX_STRING_LENGTH);  
    strncpy(settings.pzem_id, custom_pzem_id.getValue(), MAX_STRING_LENGTH);  
    strncpy(settings.idx_power, custom_idx_power.getValue(), MAX_STRING_LENGTH);  
    strncpy(settings.idx_voltage, custom_idx_voltage.getValue(), MAX_STRING_LENGTH);
    settings.AP = 0 ;  
    EEPROM.put(addr, settings); //write data to array in ram 
    EEPROM.commit();  //write data from ram to flash memory. Do nothing if there are no changes to EEPROM data in ram
}

void read_Settings () { // From EEPROM
    unsigned int addr=0 ;  
    //Serial.println("[READ EEPROM] read_Settings");  
    EEPROM.get(addr, settings); //read data from array in ram and cast it to settings
    Serial.println("[READ EEPROM] Oled name : " + String(settings.name) ) ;
    Serial.println("[READ EEPROM] mqtt_server : " + String(settings.mqtt_server) ) ;
    Serial.println("[READ EEPROM] mqtt_port : " + String(settings.mqtt_port) ) ;
    Serial.println("[READ EEPROM] pzem_topic : " + String(settings.pzem_topic) ) ;
    Serial.println("[READ EEPROM] pzem_id : " + String(settings.pzem_id) ) ;
    Serial.println("[READ EEPROM] idx_power : " + String(settings.idx_power) ) ;
    Serial.println("[READ EEPROM] idx_voltage : " + String(settings.idx_voltage) ) ;
    Serial.println("[READ EEPROM] power_max : " + String(settings.AP) ) ;
}

void wifi_connect () {
    // Wait for connection (even it's already done)
    while (WiFi.status() != WL_CONNECTED) {
        oled_cls(1);
        display.println("Connecting");
        display.println("wifi");
        display.display();
        delay(250);
        Serial.print(".");
        delay(250);
    }

    Serial.println("");
    Serial.print("Wifi Connected");
    Serial.println("");
    Serial.print("Connected to Network : ");
    Serial.println(WiFi.localIP());  //IP address assigned to ESP
    oled_cls(1);
    display.println("Wifi on");
    display.println(WiFi.localIP());
    display.display();
}

void setup_wifi () {
    delay(10);
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP   
    // WiFi.setSleepMode(WIFI_NONE_SLEEP); // disable sleep, can improve ap stability

    // wm.debugPlatformInfo();
    //reset settings - for testing
    // wm.resetSettings();
    // wm.erase();  

    // setup some parameters
    WiFiManagerParameter custom_html("<p>EEPROM Custom Parameters</p>"); // only custom html
    wm.addParameter(&custom_html);
    wm.addParameter(&custom_name);   
    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_mqtt_port);
    wm.addParameter(&custom_pzem_topic);
    wm.addParameter(&custom_pzem_id);
    wm.addParameter(&custom_idx_power);
    wm.addParameter(&custom_idx_voltage);
    // callbacks
    //wm.setAPCallback(configModeCallback);
    wm.setSaveConfigCallback(saveWifiCallback);
    wm.setBreakAfterConfig(true); // needed to use saveWifiCallback
    
    // set values later if I want
    // custom_html.setValue("test",4);
    // custom_token.setValue("test",4);

    //sets timeout until configuration portal gets turned off
    //useful to make it all retry or go to sleep in seconds
    wm.setConfigPortalTimeout(120);
    WiFi.printDiag(Serial);
    if(!wm.autoConnect("pzem_AP","admin")) {
        Serial.println("failed to connect and hit timeout");
    } 
    else if(TEST_CP or settings.AP) {
        // start configportal always
        delay(1000);
        Serial.println("AP Config Portal");
        wm.setConfigPortalTimeout(TESP_CP_TIMEOUT);
        wm.startConfigPortal("req_pzem_AP");
    }
    else {
        //Here connected to the WiFi
        Serial.println("connected...yeaaa :)");
    }
    WiFi.printDiag(Serial);
}

bool mqtt_connect(int retry) {
    bool ret = false ;
    while (!mqtt_client.connected() && WiFi.status() == WL_CONNECTED && retry) {
        String clientId = "pzem-"+String(settings.pzem_id);
        Serial.print("[mqtt_connect] (re)connecting (" + String(retry) + ") ") ;
        retry--;
        Serial.println("[mqtt_connect]"+String(settings.mqtt_server)+":"+String(settings.mqtt_port)) ; 
        oled_cls(1);
        display.println("Connecting");
        display.println("mqtt - (" + String(retry)+")");  
        display.println("idx_p :" + String (settings.idx_power) );
        display.println("idx_v :" + String (settings.idx_voltage) );
        display.display();
        if (!mqtt_client.connect(clientId.c_str())) {
            ret = false ;
            delay(5000);
        } else {
            ret = true ;
            Serial.println("[mqtt_connect] Subscribing : "+ cmdTopic) ; 
            delay(2000);
            mqtt_client.subscribe(cmdTopic.c_str());

        }
    }
    return ret ;
}

void bootPub() {
        String  msg = "{\"type\": \"pzem\"";	
                msg += ", \"id\": ";
                msg += "\"" + String(settings.pzem_id) + "\"" ;
                msg += ", \"fw_version\": ";
                msg += "\"" + String(FW_VERSION) + "\"" ;
                msg += ", \"pzem_version\": ";
                msg += "\"v3.0\"" ;
                msg += ", \"pzem_idx1\": ";
                msg += "\"" + String(settings.idx_power) + "\"" ;
                msg += ", \"pzem_idx2\": ";
                msg += "\"" + String(settings.idx_voltage) + "\"" ;
                msg += ", \"ip\": ";  
                msg += WiFi.localIP().toString().c_str() ;
                msg += "}" ;
        Serial.println("Sending boot on topic : "); Serial.print(String(settings.pzem_topic));
        mqtt_client.publish(String(settings.pzem_topic).c_str(), msg.c_str()); 
}

void domoPub(String idx, float value) {
      String msg = "{\"idx\": ";	 // {"idx": 209, "nvalue": 0, "svalue": "2052"}
      msg += idx;
      msg += ", \"nvalue\": 0, \"svalue\": \"";
      msg += value ;
      msg += "\"}";

      String domTopic = DOMO_TOPIC;             // domoticz topic
      mqtt_client.publish(domTopic.c_str(), msg.c_str()); 
}

void statusPub(float voltage, float current, float power, float energy, float frequency, float pf ) {
    String msg = "{";
    msg += "\"voltage\": ";
    msg += String(voltage);
    msg += ", \"current\": ";
    msg += String(current);
    msg += ", \"power\": ";
    msg += String(power);
    msg += ", \"energy\": ";
    msg += String(energy);
    msg += ", \"frequency\": ";
    msg += String(frequency);
    msg += ", \"powerfactor\": ";
    msg += String(pf);
    msg += "}";
    String topic = String(settings.pzem_topic)+"/"+String(settings.pzem_id) ;
    mqtt_client.publish(String(topic).c_str(), msg.c_str()); 
}

void rebootOnAP(){
        Serial.println("Force Rebooting on Acess Point");
        settings.AP = 1 ;
        unsigned int addr=0 ;
        EEPROM.put(addr, settings); //write data to array in ram 
        EEPROM.commit();  // write data from ram to flash memory. Do nothing if there are no changes to EEPROM data in ram
        ESP.restart();    // call AP directly doesn't works cleanly, a reboot is needed
}

void on_message(char* topic, byte* payload, unsigned int length) {
    if (DEBUG) { Serial.println("receiving msg on "); Serial.print(String(topic));}; 
    char buffer[length+1];
    memcpy(buffer, payload, length);
    buffer[length] = '\0';
    float p = String(buffer).toFloat();
    if(p == 999) {    // Special cmd : AccessPoint is requested - 
        rebootOnAP();
    }
}

void setup() {  
    
    randomSeed(micros());  // initializes the pseudo-random number generator
    Serial.begin(115200);

    // Pzem
    pzem1 = PZEM004Tv30(pzemSWSerial1) ; 
    //pzem1.resetEnergy()    // can be implemented on Mqtt rx message

    // OLED Shield 
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
    display.display();
    if (DEBUG) {delay(10000);} 
    
    //load eeprom data (sizeof(settings) bytes) from flash memory into ram
    EEPROM.begin(sizeof(settings));
    Serial.println("EEPROM size: " + String(sizeof(settings)) + " bytes");
    read_Settings(); // read EEPROM
    
    setup_wifi() ;
    delay(5000) ;
    uint16_t port ;
    char *ptr ;
    port = strtoul(settings.mqtt_port,&ptr,10) ;
    cmdTopic = String(settings.pzem_topic) + "/" + String(settings.pzem_id) +"/cmd";  //e.g topic :regul/vload/id-00/cmd
    outTopic = String(settings.pzem_topic) + "/" + String(settings.pzem_id) ;         //e.g topic :regul/vload/id-00/

    mqtt_client.setServer(settings.mqtt_server, port); // data will be published
    mqtt_client.setCallback(on_message); // subscribing/listening mqtt cmdTopic
    // OTA 
    #ifdef USEOTA
    webota.init(8080,"/update"); // Init WebOTA server 
    #endif
}

void loop() {
    unsigned long startTime = millis();
    if (WiFi.status() != WL_CONNECTED) {
        wifi_connect();
    }
    if (!mqtt_client.connected() && WiFi.status() == WL_CONNECTED ) {
       if (mqtt_connect(MQTT_RETRY)) { 
            bootPub();
        } else {
            rebootOnAP();
        }
    }
    mqtt_client.loop(); // seems it blocks for 100ms
    Serial.print("PZEM-1 Custom Address:");
    Serial.println(pzem1.readAddress(), HEX);
    
    //read energymeter 1  
    float voltage = pzem1.voltage();
    float current = pzem1.current();
    float power = pzem1.power();
    float energy = pzem1.energy();
    float frequency = pzem1.frequency();
    float pf = pzem1.pf(); 
    Serial.print("1-Puissance : ") ;
    Serial.println(power);
    Serial.print("1-Volt : ") ;
    Serial.println(voltage);
    // Serial.print("1-Ampere : ") ;
    // Serial.println(current);
    // Serial.print("1-Energie : ") ;
    // Serial.println(energy);
    // Serial.print("1-Fréquence : ") ;
    // Serial.println(frequency);
    // Serial.print("1-Power Factor :") ;  
    // Serial.println(pf); // PowerFactor
    Serial.println("--") ;
    oled_cls(1);
    if(isnan(voltage)) {
                Serial.println("Error reading voltage");
                display.println("Voltage");
                display.println("PZEM err.");
            } else if (isnan(current)) {
                Serial.println("Error reading current");
                display.println("Current");
                display.println("PZEM err.");
            } else if (isnan(power)) {  
                Serial.println("Error reading power");
                display.println("Power");
                display.println("PZEM err.");
            } else if (isnan(energy)) { 
                Serial.println("Error reading energy");
                display.println("Energy");
                display.println("PZEM err.");
            } else if (isnan(frequency)) {
                Serial.println("Error reading frequency");
                display.println("Frequency");
                display.println("PZEM err.");
            } else if (isnan(pf)) {
                Serial.println("Error reading power factor");
                display.println("PFactor");
                display.println("PZEM err.");
            } else {  
                    statusPub(voltage, current, power, energy, frequency, pf);
                    domoPub(String(settings.idx_power),power);
                    domoPub(String(settings.idx_voltage),voltage);
                    display.println("CONSO :");
                    display.println("");
                    display.print(String(power));
                    display.println(" W");
                    display.println("");
                    display.print(String(frequency));
                    display.println(" Hz");
                    display.print(String(voltage));
                    display.println(" V");
                    }
    display.display();

    #ifdef USEOTA
    webota.handle(); 
    webota.delay(PERIOD_V30);
    #else
    delay(PERIOD_V30) ;
    #endif

    if (DEBUG) {
        Serial.print("loop time (ms) : ") ;
        Serial.println((millis()-startTime)); // print spare time in loop 
        // delay(10000);
        }   
    
}

