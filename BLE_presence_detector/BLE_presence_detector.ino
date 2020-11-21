#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEBeacon.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Arduino.h>
#include <WiFi.h>

// ---------------- Configurações---------------


#define DEVICE_NAME "BLE_presence_detector"

#define SCAN_TIME 2 // Time between each scan/tick in seconds
#define SEEN_TICKS 10 // How many ticks until a device is considered gone
#define RSSI_THRESHOLD -80 // Signals below this will be filtered
#define WIFI_WAIT 90 // Amount/2 of time to wait for WiFi before resetting everything (ex: 90 == 45 seconds)
#define BROKER_WAIT 30 // Amount of time in seconds to wait for the broker to connect before resetting everything 

// -------------- Fim das configurações---------------


#define foreach_tracked_address() for(size_t i = 0; i<sizeof(tracked_addresses)/sizeof(tracked_addresses[0]); i++)

BLEScan* pBLEScan;
WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);


int last_seens[sizeof(tracked_addresses)/sizeof(tracked_addresses[0])];

String getAddrName(BLEAddress addr){
    String addr_str = String(addr.toString().c_str());
    addr_str.replace(":","-");
    return "ble_presence_" + addr_str;
}

void mqtt_publish(BLEAddress addr, String payload){
    mqtt_client.publish(("homeassistant/binary_sensor/"+getAddrName(addr)+"/state").c_str(), payload.c_str());
}

void mqtt_register(){
    foreach_tracked_address(){
        BLEAddress addr = tracked_addresses[i];
        String name = getAddrName(addr);
        String payload = "{\"name\": \""+name+"\", \"device_class\": \"presence\", \"state_topic\": \"homeassistant/binary_sensor/"+name+"/state\"}";
        mqtt_client.publish(("homeassistant/binary_sensor/"+name+"/config").c_str(), payload.c_str());
    }
}

void tick(){
    Serial.println("tick:");
    foreach_tracked_address(){
        if (last_seens[i]){
            last_seens[i]--;
            if(last_seens[i] == 0){
                mqtt_publish(tracked_addresses[i], "OFF");
                printAddress(i, " is gone!");
            }
        }
        printAddress(i, "tick");
    }
}

void printAddress(size_t index, String extra_text){
    Serial.print(tracked_addresses[index].toString().c_str());
    Serial.print(" ");
    Serial.print(last_seens[index]);
    Serial.print(" ");
    Serial.println(extra_text);
}

void seen(BLEAddress search_addr){
    foreach_tracked_address(){
        if(search_addr.equals(tracked_addresses[i])){
            if (!last_seens[i]){
                mqtt_publish(search_addr, "ON");
                printAddress(i, "found!");
            }
            last_seens[i] = SEEN_TICKS;
            return;
        }
    }
    Serial.printf("Unknown device: %s\n", search_addr.toString().c_str());
    
}
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
	    void onResult(BLEAdvertisedDevice advertisedDevice) {
	    	// RSSI = Received signal strength indication
	    	if (advertisedDevice.getRSSI() > RSSI_THRESHOLD){
                seen(advertisedDevice.getAddress());
            }
            else{
	    	    Serial.printf("Device filtered: %s  RSSI: %d\n", advertisedDevice.getAddress().toString().c_str(), advertisedDevice.getRSSI());
            }
	    }
};

void setup() {
	Serial.begin(115200);

    WiFi.begin(SSID, PASSWORD);
    
    // Connect to WiFi
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Waiting for Wi-Fi connection...");
        if(tries++ == WIFI_WAIT) ESP.restart();
    }

    // set all devices to not seen
    foreach_tracked_address(){
        last_seens[i] = 0;
    }

	Serial.println("Starting scan...");
	BLEDevice::init(DEVICE_NAME);
	pBLEScan = BLEDevice::getScan(); //create new scan
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
	pBLEScan->setActiveScan(false); //active scan uses more power, but get results faster
	pBLEScan->setInterval(100);
	pBLEScan->setWindow(90);  // less or equal setInterval value
}
void mqtt_reconnect(){
     // Connect to the broker
    int tries = 0;
    mqtt_client.setServer(MQTTSERVER, MQTTPORT);
    while (!mqtt_client.connected()){
        Serial.println("Connecting to MQTT broker...");
        if (mqtt_client.connect(DEVICE_NAME, MQTTUSER, MQTTPASSWORD )){
            Serial.println("broker connected");
        }
        else {
            Serial.print("Failed to connect to broker: ");
            Serial.print(mqtt_client.state());
            delay(2000);
        }
        if(tries++ == BROKER_WAIT) ESP.restart();
    }
    mqtt_register();
}

void loop() {
    if ( WiFi.status() != WL_CONNECTED){
        // If we lost WiFi connection, fuck it, just reset everything and try again.
        Serial.printf("Lost WiFi connection!");
        ESP.restart();
    }
    if (!mqtt_client.loop()){
        // But if its just mqtt, try to reconect
        Serial.printf("Lost MQTT connection! Trying to reconnect...");
        mqtt_reconnect();
    }
	BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME, false);
    tick();
	Serial.println("Scan done!");
	pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
}
