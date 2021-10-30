const char* ssid = "";
const char* password = "";

const char* mqtt_server = "test.mosquitto.org";
const char* topic_prefix = "pzem" ;


// Unique idx of module, which is used to build the domoticz MQTT message on which data are published on Domoticz/in topic
#define DOMO_TOPIC "domoticz/in" 
#define PZEM1_IDX1 888  // domoticz power idx 
#define PZEM1_IDX2 999  // domoticz voltage idx

