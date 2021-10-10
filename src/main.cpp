#include <Arduino.h>
#include <PZEM004Tv30.h>      // https://github.com/mandulaj/PZEM-004T-v30
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>     // Mqtt lib
#include <myConfig.h>         // WiFi and MQTT settings in this header : NOT 'Gited'
#include <SoftwareSerial.h>   // ESP8266/wemos requirement
#include "Adafruit_SSD1306.h" // OLED requirement

#define myDEBUG true

// Unique id of module, which is used to build the MQTT topic on which data are published.
#define PZEM_TOPIC "pzem" 
#define PZEM1_ID "Cons"
#define PZEM2_ID "Prod"

#define FW_VERSION "1.0"

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

unsigned long myTime;

void oled_cls(int size) {
    // OLED : set cursor on top left corner and clear
    display.clearDisplay();
    display.setTextSize(size);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
}

void setup_wifi () {
  delay(10);
  WiFi.softAPdisconnect (true);
  WiFi.begin(ssid, password);
  // Wait for connection
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
  Serial.print("Connected to Network/SSID : ");
  Serial.println(ssid) ;
  Serial.print("IP address : ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP
  oled_cls(1);
  display.println("Wifi on");
  display.println(WiFi.localIP());
  display.display();
}

void mqtt_connect() {
    while (!mqtt_client.connected()) {
        String clientId = "pzem";
        Serial.print("Mqtt (re)connecting ") ;
        Serial.println(mqtt_server) ; 
        oled_cls(1);
        display.println("Connecting");
        display.println("mqtt");  
        display.println("idx_v :" + String (PZEM1_IDX1) );
        display.println("idx_p :" + String (PZEM1_IDX2) );
        display.display();
        if (!mqtt_client.connect(clientId.c_str())) {
            delay(5000);
        }
    }
}

void bootPub() {
        String  msg = "{\"type\": \"boot\"";	
                msg += ", \"fw_version\": ";
                msg += "\"" + String(FW_VERSION) + "\"" ;
                msg += ", \"pzem1_version\": ";
                msg += "\"v3.0\"" ;
                msg += ", \"pzem1_idx1\": ";
                msg += "\"" + String(PZEM1_IDX1) + "\"" ;
                msg += ", \"pzem1_idx2\": ";
                msg += "\"" + String(PZEM1_IDX2) + "\"" ;
                msg += "}" ;
        mqtt_client.publish(String(PZEM_TOPIC).c_str(), msg.c_str()); 
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

void pzemPub(String id, float voltage, float current, float power, float energy, float frequency, float pf ) {
      String msg = "{\"type\": \"" + String("pzem-") + id +"\"";	
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

      String outTopic = PZEM_TOPIC + String("/pzem-");  // pzem id  topic
      outTopic += id ;
      mqtt_client.publish(outTopic.c_str(), msg.c_str()); 
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

  setup_wifi() ;
  delay(5000) ;

  mqtt_client.setServer(mqtt_server, 1883); // data will be published
  delay(5000) ;
  
  if (!mqtt_client.connected()) {
      mqtt_connect();
      bootPub();
      delay(5000) ;
    }
}

void loop() {

    if (!mqtt_client.connected()) {
      mqtt_connect();
      bootPub();
    }

    mqtt_client.loop();
    Serial.print("Time: ");
    myTime = millis();

    Serial.println(myTime);
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
                    pzemPub(String(PZEM1_ID),voltage, current, power, energy, frequency, pf);
                    domoPub(String(PZEM1_IDX1),power);
                    domoPub(String(PZEM1_IDX2),voltage);
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
        Serial.println(myTime); // prints time since program started 
        // delay(10000);
        } 
}

