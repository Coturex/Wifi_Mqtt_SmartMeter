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
#include <WiFiManager.h>        // Manage Wifi Access Point if wifi connect failure 
#include "Adafruit_SSD1306.h"   // OLED requirement

#define USEOTA // enable On The Air firmware flash 
#ifdef USEOTA
#include "WebOTA.h"
#endif

//#include "myConfig_sample.h"  // Personnal settings - 'gited file'
//#include "myConfig.h"           // Personnal settings - Not 'gited file'
#include <EEPROM.h>             // EEPROM access...
#define FW_VERSION "1.2"
#define DOMO_TOPIC "domoticz/in" 

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

// *****************************************************
// * WHICH PZEM VERSION IS USED ?
// *******************************************************
//#define USE_PZEM_V2 // LET COMMENT IF USE PZEM VERSION 3
#undef USE_PZEM_V2 // LET COMMENT IF USE PZEM VERSION 2
// *******************************************************

// Wait this duration between each measurement (milliseconds). This is added to the time needed to read data (~2s)
#define PERIOD_PZEM 1000  // default PZEM wait duration (a sample rate can be added, cf domoPubTimer)
#ifdef USE_PZEM_V2
#include "PZEM004Tv20.h"      // comment if not used
PZEM004T pzem1(D6,D5);  // (RX,TX) connect to RX,TX of PZEM
IPAddress pzem_ip(192,99,1,1);
#else
#include "PZEM004Tv30.h"        // comment if not used
SoftwareSerial pzemSWSerial1(D5, D6);
PZEM004Tv30 pzem1 ; // (TX,RX)connect to TX,RX of PZEM
#endif

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
    int  domoPubTimer = 0;
    int  AP = 0 ;
  } settings;

struct {
    float voltage = 0 ;
    float current = 0 ;
    float power = 0 ;
    float energy = 0 ;
    float frequency = 0 ;
    float pf= 0;
    float powerAvg = 0 ;
    bool  read_error = false ;
} pzemValues ;

WiFiManager wm;
WiFiManagerParameter custom_name("name", "Oled Title", "", 15);
WiFiManagerParameter custom_mqtt_server("mqtt_server", "mqtt IP server", "", 15);
WiFiManagerParameter custom_mqtt_port("mqtt_port", "mqtt Port", "", 4);
WiFiManagerParameter custom_pzem_topic("pzem_topic", "Pzem Topic", "", 35);
WiFiManagerParameter custom_pzem_id("pzem_id", "Pzem ID", "", 15);
WiFiManagerParameter custom_idx_power("idx_power", "Domoticz idx power", "", 4); 
WiFiManagerParameter custom_idx_voltage("idx_volt", "Domoticz idx voltage", "", 4);
WiFiManagerParameter custom_domoPubTimer("domoPubTimer", "Domoticz publish timer (s)", "", 4);

// domoPubTimer is Used to reduce telemetry sent to Domoticz/InfluxDb
//      if empty -> synchronise Domoticz publication on PZEM sample rate
//      if x sec -> Domoticz publication every x seconds (Power is then averaged). 

void oled_cls(int size) {
    // OLED : set cursor on top left corner and clear
    display.clearDisplay();
    display.setTextSize(size);
    display.setTextColor(WHITE); // seems only WHITE exist on this oled model :(
    display.setCursor(0,0);
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
    Serial.println("[READ EEPROM] domoPubTimer   : " + String(settings.domoPubTimer) ) ;
    }

void saveWifiCallback() { // Save settings to EEPROM
    unsigned int addr=0 ;
    String srate_str ;
    read_Settings();
    Serial.println("[CALLBACK] saveParamCallback fired"); 
    // Field statint with '.' is ignored, value is not updated
    if (custom_name.getValue()[0] != '.') { 
        strncpy(settings.name, custom_name.getValue(), MAX_STRING_LENGTH);  
    }
    if (custom_name.getValue()[0] != '.') { 
    strncpy(settings.mqtt_server, custom_mqtt_server.getValue(), MAX_STRING_LENGTH);  
    }
    if (custom_mqtt_port.getValue()[0] != '.') { 
        strncpy(settings.mqtt_port, custom_mqtt_port.getValue(), MAX_STRING_LENGTH);  
    }
    if (custom_pzem_topic.getValue()[0] != '.') { 
        strncpy(settings.pzem_topic, custom_pzem_topic.getValue(), MAX_STRING_LENGTH);  
    }
    if (custom_pzem_id.getValue()[0] != '.') { 
        strncpy(settings.pzem_id, custom_pzem_id.getValue(), MAX_STRING_LENGTH);  
    }
    if (custom_idx_power.getValue()[0] != '.') { 
        strncpy(settings.idx_power, custom_idx_power.getValue(), MAX_STRING_LENGTH);  
    }
    if (custom_idx_voltage.getValue()[0] != '.') { 
        strncpy(settings.idx_voltage, custom_idx_voltage.getValue(), MAX_STRING_LENGTH);
    }
    if (custom_domoPubTimer.getValue()[0] != '.') { 
        char timerChar[MAX_STRING_LENGTH] = "";
        strncpy(timerChar, custom_domoPubTimer.getValue(), MAX_STRING_LENGTH);
        settings.domoPubTimer = String(timerChar).toInt() * 1000 ;
    }
    settings.AP = 0 ;  
    EEPROM.put(addr, settings); //write data to array in ram 
    EEPROM.commit();  //write data from ram to flash memory. Do nothing if there are no changes to EEPROM data in ram
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
    wm.addParameter(&custom_domoPubTimer);
    // callbacks
    //wm.setAPCallback(configModeCallback);
    wm.setSaveConfigCallback(saveWifiCallback);
    wm.setBreakAfterConfig(true); // needed to use saveWifiCallback
    
    // set values later if I want
    // custom_html.setValue("test",4);
    // custom_token.setValue("test",4);

    //sets timeout until configuration portal gets turned off
    //useful to make it all retry or go to sleep in seconds
    wm.setConfigPortalTimeout(120); // run AccessPoint for 120s
    WiFi.printDiag(Serial);
    if(!wm.autoConnect("pzem_AP","admin")) {
        Serial.println("failed to connect and hit timeout");
    } 
    else if(TEST_CP or settings.AP) {
        // start configportal always
        delay(1000);
        wm.setConfigPortalTimeout(TESP_CP_TIMEOUT);
        switch (settings.AP) {
            case 1: 
                wm.startConfigPortal("request_pzem_AP");
                Serial.println("AP Config Portal : requested on topic/cmd");
                break ;
            case 2:    
                wm.startConfigPortal("mqtt_pzem_AP");
                Serial.println("AP Config Portal : mqtt connection failure");
                break ;
        } 
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
        Serial.print("[mqtt_connect] (re)connecting (" + String(retry) + " left) ") ;
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
            delay(4000);
        } else {
            ret = true ;
            Serial.println("[mqtt_connect] Subscribing : "+ cmdTopic) ; 
            delay(2000);
            mqtt_client.subscribe(cmdTopic.c_str());
            delay(2000);
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
                #ifdef USE_PZEM_V2
                msg += "\"v2.0\"" ;
                #else
                msg += "\"v3.x\"" ;
                #endif               
                msg += ", \"pzem_idx1\": ";
                msg += "\"" + String(settings.idx_power) + "\"" ;
                msg += ", \"pzem_idx2\": ";
                msg += "\"" + String(settings.idx_voltage) + "\"" ;
                msg += ", \"DomoPubTimer\": ";
                msg += "\"" + String(settings.domoPubTimer/1000) + "\"" ;
                msg += ", \"ip\": ";  
                msg += WiFi.localIP().toString().c_str() ;
                msg += "}" ;
        if (DEBUG) { Serial.println("Sending Bootstrap on topic : " + String(settings.pzem_topic));}
        mqtt_client.publish(String(settings.pzem_topic).c_str(), msg.c_str()); 
}

void domoPub(String idx, float value) {
    if (idx.toInt()> 0) {
      String msg = "{\"idx\": ";	 // {"idx": 209, "nvalue": 0, "svalue": "2052"}
      msg += idx;
      msg += ", \"nvalue\": 0, \"svalue\": \"";
      msg += value ;
      msg += "\"}";

      String domTopic = DOMO_TOPIC;             // domoticz topic
      if (DEBUG) {
        Serial.println("domoPub on topic : " + domTopic);
        Serial.println("domoPub : " + msg);
      }   
      mqtt_client.publish(domTopic.c_str(), msg.c_str()); 
    }
}

void statusPub() {
    String msg ;
    if (pzemValues.read_error) {
        msg = "{ \"PZEM_READ_ERROR\" } ";
    } else {
        msg = "{";
        msg += "\"voltage\": ";
        msg += String( (pzemValues.voltage < 0) ? 0 : pzemValues.voltage ) ;
        msg += ", \"current\": ";
        msg += String( (pzemValues.current < 0) ? 0 : pzemValues.current );
        msg += ", \"power\": ";
        msg += String( (pzemValues.power < 0) ? 0 : pzemValues.power );
        msg += ", \"energy\": ";
        msg += String( (pzemValues.energy < 0) ? 0 : pzemValues.energy );
        msg += ", \"frequency\": ";
        msg += String(pzemValues.frequency);
        msg += ", \"powerfactor\": ";
        msg += String(pzemValues.pf);
        msg += "}";
    }
    String topic = String(settings.pzem_topic)+"/"+String(settings.pzem_id) ;
    if (DEBUG) {
        Serial.println("statusPub on topic : " + topic);
        Serial.println("statusPub : " + msg);
      } 
    mqtt_client.publish(String(topic).c_str(), msg.c_str()); 
}

void rebootOnAP(int ap){
        Serial.println("Force Rebooting on Acess Point");
        settings.AP = ap ;
        unsigned int addr=0 ;
        EEPROM.put(addr, settings); //write data to array in ram 
        EEPROM.commit();  // write data from ram to flash memory. Do nothing if there are no changes to EEPROM data in ram
        ESP.restart();    // call AP directly doesn't works cleanly, a reboot is needed
}

void on_message(char* topic, byte* payload, unsigned int length) {
    if (DEBUG) { Serial.println("Receiving msg on topic : " + String(topic));}; 
    char buffer[length+1];
    memcpy(buffer, payload, length);
    buffer[length] = '\0';
    if (DEBUG) { Serial.println("  msg : {" + String(buffer) + "}");}; 
    if (String(buffer) == "bs") { // Bootstrap is requested
            if (DEBUG) { Serial.println("     Bootstrap resquested") ; } ;
            bootPub();
    } else if (String(buffer) == "ap") { // AccessPoint is requested
            if (DEBUG) { Serial.println("     AccessPoint resquested") ; } ;
            rebootOnAP(1);
    } else if (String(buffer) == "reboot") { // Reboot is requested
            if (DEBUG) { Serial.println("     Reboot resquested") ; } ;
            ESP.restart(); 
    } else if (String(buffer) == "reset") { // Reset Energy counter is requested
            #ifdef USE_PZEM_V3
            if (DEBUG) { Serial.println("     Reset Energy resquested") ; } ;
            pzem1.resetEnergy()   ;
            #endif
    } else {
        if (DEBUG) { Serial.println("     Nothing to do") ; } ;
    }
}

void setup() {  
    randomSeed(micros());  // initializes the pseudo-random number generator
    Serial.begin(115200);

    #ifdef USE_PZEM_V2
    pzem1.setAddress(pzem_ip);
    #else // USE PZEM V3
    pzem1 = PZEM004Tv30(pzemSWSerial1) ; 
    //pzem1.resetEnergy()    ;
    #endif


    // OLED Shield 
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
    display.display();
    if (DEBUG) {delay(10000);} 
    
    //load eeprom data (sizeof(settings) bytes) from flash memory into ram
    EEPROM.begin(sizeof(settings));
    Serial.println("EEPROM size: " + String(sizeof(settings)) + " bytes");
    read_Settings(); // read EEPROM
    // PERIOD_PZEM += settings.domoPubTimer ;

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

bool readPZEM(){
    #ifdef USE_PZEM_V2
        pzemValues.voltage = pzem1.voltage(pzem_ip);
        pzemValues.current = pzem1.current(pzem_ip);
        pzemValues.power = pzem1.power(pzem_ip);
        pzemValues.energy = pzem1.energy(pzem_ip);
        pzemValues.frequency = 0 ;
        pzemValues.pf = 0 ;
    #else // USE PZEM V3
        Serial.print("PZEM-1 Custom Address:");
        Serial.println(pzem1.readAddress(), HEX);
        pzemValues.power = pzem1.power();
        pzemValues.voltage = pzem1.voltage();
        pzemValues.current = pzem1.current();
        pzemValues.energy = pzem1.energy();
        pzemValues.frequency = pzem1.frequency();
        pzemValues.pf = pzem1.pf(); 
    #endif
    if (DEBUG) {
    Serial.print("1-Puissance : ") ;
    Serial.println(pzemValues.power);
    Serial.print("1-Volt : ") ;
    Serial.println(pzemValues.voltage);
    }
/*    
    Serial.print("1-Ampere : ") ;
    Serial.println(pzemValues.current);
    Serial.print("1-Energie : ") ;
    Serial.println(pzemValues.energy);
    Serial.print("1-Fréquence : ") ;
    Serial.println(pzemValues.frequency);
    Serial.print("1-Power Factor :") ;  
    Serial.println(pzemValues.pf); // PowerFactor
    */
    oled_cls(1);
    if(isnan(pzemValues.voltage)) {
                Serial.println("Error reading voltage");
                display.println("Voltage");
                display.println("PZEM err.");
                pzemValues.read_error = true ;
            } else if (isnan(pzemValues.current)) {
                Serial.println("Error reading current");
                display.println("Current");
                display.println("PZEM err.");
                pzemValues.read_error = true ;
            } else if (isnan(pzemValues.power)) {  
                Serial.println("Error reading power");
                display.println("Power");
                display.println("PZEM err.");
                pzemValues.read_error = true ;
            } else if (isnan(pzemValues.energy)) { 
                Serial.println("Error reading energy");
                display.println("Energy");
                display.println("PZEM err.");
                pzemValues.read_error = true ;
            } else if (isnan(pzemValues.frequency)) {
                Serial.println("Error reading frequency");
                display.println("Frequency");
                display.println("PZEM err.");
                pzemValues.read_error = true ;
            } else if (isnan(pzemValues.pf)) {
                Serial.println("Error reading power factor");
                display.println("PFactor");
                display.println("PZEM err.");
                pzemValues.read_error = true ;
            } else {  
                    pzemValues.read_error = false ;
                    display.println(String(settings.name));
                    display.println("");
                    display.print(String(pzemValues.power));
                    display.println(" W");
                    display.println("");
                    display.print(String(pzemValues.frequency));
                    display.println(" Hz");
                    display.print(String(pzemValues.voltage));
                    display.println(" V");
                    }   
            
    display.display();
    return !pzemValues.read_error ;
}

unsigned long startLoopDomoPubTimer = millis();
unsigned long spendTimeDomoPubTimer ;
int countLoopDomoPubTimer = 0 ;
    
void loop() {
    unsigned long loopTime = millis() ;
    if (DEBUG) {Serial.println("--") ;} ;
    if (WiFi.status() != WL_CONNECTED) {
        wifi_connect();
    }
    if (!mqtt_client.connected() && WiFi.status() == WL_CONNECTED ) {
       if (mqtt_connect(MQTT_RETRY)) { 
           bootPub();
        } else {
            rebootOnAP(2);
        }
    }
    mqtt_client.loop(); // seems it blocks for 100ms
    
    if (readPZEM()) {
        countLoopDomoPubTimer++ ;
        if (settings.domoPubTimer > PERIOD_PZEM) { // domoPubTimer settings is not Empty,  PowerAvg is used
            pzemValues.powerAvg += pzemValues.power ;
            spendTimeDomoPubTimer = millis() - startLoopDomoPubTimer ;
            if (DEBUG) { Serial.println("spendTimeDomoPubTimer : " + String(spendTimeDomoPubTimer)) ;} ; 
            if (spendTimeDomoPubTimer >=  settings.domoPubTimer) {
                domoPub(String(settings.idx_power),pzemValues.powerAvg/countLoopDomoPubTimer); 
                domoPub(String(settings.idx_voltage),pzemValues.voltage);  // last read voltage value is enough (unaveraged)
                startLoopDomoPubTimer = millis();
                countLoopDomoPubTimer = 0 ;
                pzemValues.powerAvg = 0 ;
            }
        } else { // Pzem Standard sample rate is used to publish on Domoticz topic
            domoPub(String(settings.idx_power),pzemValues.power);
            domoPub(String(settings.idx_voltage),pzemValues.voltage);            
        }
    } ;

    statusPub();

    #ifdef USEOTA
    webota.handle(); 
    webota.delay(PERIOD_PZEM);
    #else
    delay(PERIOD_PZEM) ;
    #endif

    if (DEBUG) {
        Serial.print("loop time (ms) : ") ;
        Serial.println((millis()-loopTime)); // print spare time in loop 
        }   
}

