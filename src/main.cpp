#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>       // Mqtt lib
#include <SoftwareSerial.h>     // ESP8266/wemos requirement
#include <WiFiManager.h>        // Manage Wifi Access Point if wifi connect failure (todo : and mqtt failure)
#include "PZEM004Tv30.h"        // comment if not used
//#include "PZEM004Tv20.h"      // comment if not used
#include "Adafruit_SSD1306.h"   // OLED requirement

#include <EEPROM.h>              // EEPROM access...
#define MAX_STRING_LENGTH 20         

#define myDEBUG false

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

// Setup our pzem module with these pins : pins must be chosen carefully so that the ESP can/cannot boot.
// Wemos : it's best to avoid GPIO 0, 2 and 15 (D3, D4, D8)
// Wemos must use softwareSerial method
SoftwareSerial pzemSWSerial1(D5, D6);
PZEM004Tv30 pzem1 ; // (RX,TX)connect to TX,RX of PZEM

// wifiManager TEST OPTION FLAGS
bool TEST_CP         = false; // always start the ConfigPortal, even if ap found
int  TESP_CP_TIMEOUT = 30;   // test cp timeout
bool ALLOWONDEMAND   = true; // enable on demand - e.g Trigged on Gpio Input, On Mqtt Msg etc...

struct { 
    char mqtt_server[MAX_STRING_LENGTH] = "";
    char mqtt_port[MAX_STRING_LENGTH] = "";
    char pzem_topic[MAX_STRING_LENGTH] = "";
    char pzem_id[MAX_STRING_LENGTH] = "";
    char idx_power[MAX_STRING_LENGTH] = "";
    char idx_voltage[MAX_STRING_LENGTH] = "";
  } settings;

WiFiManager wm;
WiFiManagerParameter custom_mqtt_server("mqtt_server", "mqtt IP server", "", 15);
WiFiManagerParameter custom_mqtt_port("mqtt_port", "mqtt Port", "", 4);
WiFiManagerParameter custom_pzem_topic("pzem_topic", "Pzem Topic", "", 15);
WiFiManagerParameter custom_pzem_id("pzem_id", "Pzem ID", "", 15);
WiFiManagerParameter custom_idx_power("idx_power", "Domoticz idx power", "", 4); 
WiFiManagerParameter custom_idx_voltage("idx_volt", "Domoticz idx voltage", "", 4);

void oled_cls(int size) {
    // OLED : set cursor on top left corner and clear
    display.clearDisplay();
    display.setTextSize(size);
    display.setTextColor(WHITE); // only WHITE exist on this oled model :(
    display.setCursor(0,0);
}

void saveWifiCallback() { // Save settings to EEPROM
    unsigned int addr=0 ;
    Serial.println("[CALLBACK] saveParamCallback fired"); 
    strncpy(settings.mqtt_server, custom_mqtt_server.getValue(), MAX_STRING_LENGTH);  
    strncpy(settings.mqtt_port, custom_mqtt_port.getValue(), MAX_STRING_LENGTH);  
    strncpy(settings.pzem_topic, custom_pzem_topic.getValue(), MAX_STRING_LENGTH);  
    strncpy(settings.pzem_id, custom_pzem_id.getValue(), MAX_STRING_LENGTH);  
    strncpy(settings.idx_power, custom_idx_power.getValue(), MAX_STRING_LENGTH);  
    strncpy(settings.idx_voltage, custom_idx_voltage.getValue(), MAX_STRING_LENGTH);  
    EEPROM.put(addr, settings); //write data to array in ram 
    EEPROM.commit();  //write data from ram to flash memory. Do nothing if there are no changes to EEPROM data in ram
}

void read_Settings () { // From EEPROM
    unsigned int addr=0 ;  
    //Serial.println("[READ EEPROM] read_Settings");  
    EEPROM.get(addr, settings); //read data from array in ram and cast it to settings
    Serial.println("[READ EEPROM] mqtt_server : " + String(settings.mqtt_server) ) ;
    Serial.println("[READ EEPROM] mqtt_port : " + String(settings.mqtt_port) ) ;
    Serial.println("[READ EEPROM] pzem_topic : " + String(settings.pzem_topic) ) ;
    Serial.println("[READ EEPROM] pzem_id : " + String(settings.pzem_id) ) ;
    Serial.println("[READ EEPROM] idx_power : " + String(settings.idx_power) ) ;
    Serial.println("[READ EEPROM] idx_voltage : " + String(settings.idx_voltage) ) ;
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
  
    //If connection successful show IP address in serial monitor
    Serial.println("");
    Serial.print("Connected to Network : ");
    Serial.println(WiFi.localIP());  //IP address assigned to your ESP
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
    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_mqtt_port);
    wm.addParameter(&custom_pzem_topic);
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
    else if(TEST_CP) {
        // start configportal always
        delay(1000);
        Serial.println("TEST_CP ENABLED");
        wm.setConfigPortalTimeout(TESP_CP_TIMEOUT);
        wm.startConfigPortal("test_pzem_AP");
    }
    else {
        //Here I'm connected to the WiFi
        Serial.println("connected...yeey :)");
    }
    WiFi.printDiag(Serial);
}

bool mqtt_connect() {
    bool ret = false ;
    while (!mqtt_client.connected() && WiFi.status() == WL_CONNECTED) {
        String clientId = "pzem";
        Serial.print("Mqtt (re)connecting ") ;
        Serial.println(String(settings.mqtt_server)+"@"+String(settings.mqtt_port)) ; 
        oled_cls(1);
        display.println("Connecting");
        display.println("mqtt");  
        display.println("idx_v :" + String (settings.idx_power) );
        display.println("idx_p :" + String (settings.idx_voltage) );
        display.display();
        if (!mqtt_client.connect(clientId.c_str())) {
            delay(5000);
            ret = true ;
        }
    }
    return ret ;
}

void bootPub() {
        String  msg = "{\"type\": \"boot\"";	
                msg += ", \"fw_version\": ";
                msg += "\"" + String(FW_VERSION) + "\"" ;
                msg += ", \"pzem1_version\": ";
                msg += "\"v3.0\"" ;
                msg += ", \"pzem1_idx1\": ";
                msg += "\"" + String(settings.idx_power) + "\"" ;
                msg += ", \"pzem1_idx2\": ";
                msg += "\"" + String(settings.idx_voltage) + "\"" ;
                msg += "}" ;
        mqtt_client.publish(String(settings.pzem_topic).c_str(), msg.c_str()); 
}

void domoPub(String idx, float value) {
      String msg = "{\"idx\": ";	 // {"idx": 209, "nvalue": 0, "svalue": "2052"}
      msg += idx;
      msg += ", \"nvalue\": 0, \"svalue\": \"";
      msg += value ;
      msg += "\"}";

      String outTopic = DOMO_TOPIC;             // domoticz topic
      mqtt_client.publish(outTopic.c_str(), msg.c_str()); 
}

void pzemPub(float voltage, float current, float power, float energy, float frequency, float pf ) {
      String msg = "{\"type\": \"pzem\"";	
      msg += ", \"voltage\": ";
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
      String topic = String(settings.pzem_topic)+"/pzem-"+String(settings.pzem_id) ;
      mqtt_client.publish(String(topic).c_str(), msg.c_str()); 
}

void setup() {  
    
    randomSeed(micros());  // initializes the pseudo-random number generator

    Serial.begin(115200);
    pzem1 = PZEM004Tv30(pzemSWSerial1) ; 
    //pzem1.resetEnergy()    // can be implemented on Mqtt rx message
    // OLED Shield 
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
    display.display();
    if (myDEBUG) {delay(10000);} 
    //load eeprom data (sizeof(settings) bytes) from flash memory into ram
    EEPROM.begin(sizeof(settings));
    Serial.println("EEPROM size: " + String(sizeof(settings)) + " bytes");
    read_Settings();
    setup_wifi() ;
    delay(5000) ;
    uint16_t port ;
    char *ptr ;
    port = strtoul(settings.mqtt_port,&ptr,10) ;
    mqtt_client.setServer(settings.mqtt_server, port); // data will be published
    Serial.println("*****" + String(port));
    delay(5000) ;
}

void loop() {
    unsigned long startTime;
    if (WiFi.status() != WL_CONNECTED) {
        wifi_connect();
    }
    if (!mqtt_client.connected() && WiFi.status() == WL_CONNECTED) {
        if (mqtt_connect()) {
            bootPub();
        }
    }
    mqtt_client.loop(); // seems blocks for 100ms
    Serial.print("start Time: ");
    startTime = millis();

    Serial.println(startTime);
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
                    pzemPub(voltage, current, power, energy, frequency, pf);
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
    delay(PERIOD_V30) ;

    if (myDEBUG) {
        Serial.println(startTime-millis()); // prints time since program started 
        // delay(10000);
        } 
}

