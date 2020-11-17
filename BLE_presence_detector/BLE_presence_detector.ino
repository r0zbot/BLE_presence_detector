#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Arduino.h>
#include <WiFi.h>


#define SCAN_TIME 1 // Time between each scan/tick in seconds
#define SEEN_TICKS 10 // How many ticks until a device is considered gone
#define RSSI_THRESHOLD -80 // Signals below this will be filtered
#define MAX_LEN 100 // Maximum amount of tracked MAC addresses
#define WIFI_WAIT 90 // Amount/2 of time to wait for WiFi before resetting everything (ex: 90 == 45 seconds)
#define BROKER_WAIT 30 // Amount of time in seconds to wait for the broker before resetting everything 

//TODO document gitignored stuff
#define DEVICE_NAME "BLE_presence_detector"

#define TOPIC_LEAVE "BLE_presence_detector/leave"
#define TOPIC_JOIN "BLE_presence_detector/join"

BLEScan* pBLEScan;
WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);

struct AddressEntry {
    esp_bd_addr_t addr;
    int last_seen; // when it reaches 0, the device is considered to be gone
};
class AddressList {
    public:
        size_t get_index(BLEAddress search_addr){
            for(size_t i = 0; i<addr_len; i++){
                if(!addr_storage[i].last_seen){
                    continue;
                }

                BLEAddress addr =  BLEAddress(addr_storage[i].addr);
                if (addr.equals(search_addr)){
                    return i;
                }
            }
            return -1;
        }

        void tick(){
            for(size_t i = 0; i<addr_len; i++){
                if (addr_storage[i].last_seen){
                    addr_storage[i].last_seen--;
                    if(addr_storage[i].last_seen == 0){
                        BLEAddress addr =  BLEAddress(addr_storage[i].addr);
                        mqtt_client.publish(TOPIC_LEAVE, addr.toString().c_str());
                        Serial.printf("%s remove\n", toString(addr_storage[i]).c_str());
                        continue;
                    }
                }
            }
        }

        bool seen(BLEAddress search_addr){
            size_t empty_space = 0; // this should point to the first empty space if it exists
            for(size_t i = 0; i<addr_len; i++){
                if(!addr_storage[i].last_seen){
                    if(!empty_space) empty_space = i;
                    continue;
                }

                BLEAddress addr =  BLEAddress(addr_storage[i].addr);
                if (addr.equals(search_addr)){
                    addr_storage[i].last_seen = SEEN_TICKS; // reset last_seen status
                    Serial.printf("%s tick\n", toString(addr_storage[i]).c_str());
                    return false; // Address already exists
                }
            }
            AddressEntry entry;
            memcpy(entry.addr, search_addr.getNative(), ESP_BD_ADDR_LEN);
            entry.last_seen = SEEN_TICKS;
            if (empty_space){
                addr_storage[empty_space] = entry;
                Serial.printf("%s add (hole)\n", toString(entry).c_str());
            }
            else{
                if(addr_len < MAX_LEN){
                    Serial.printf("%s add\n", toString(entry).c_str());
                    addr_storage[addr_len++] = entry;
                }
                else{
                    //TODO: send signal for LIST FULL
                    Serial.printf("THE LIST IS FULL!\n");
                    return false; // oshit, the list is full;
                }
            }
            mqtt_client.publish(TOPIC_JOIN, search_addr.toString().c_str());

            //TODO: Send signal for discovery
            return true; // Address was added
        }

        String toString(){
            String out;
            for(size_t i = 0; i<addr_len; i++){
                if(addr_storage[i].last_seen){
                    out += toString(addr_storage[i]).c_str();
                    out += "\n";
                }
            }
            return out;
        }

        String toString(AddressEntry entry){
            String out;
            out += BLEAddress(entry.addr).toString().c_str();
            out += " ";
            out += entry.last_seen;
            return out;
        }

        size_t size(){
            // this is wrong
            return addr_len;
        }
    private:
        size_t addr_len = 0;
        AddressEntry addr_storage[MAX_LEN];
};
AddressList addr_list;
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
	    void onResult(BLEAdvertisedDevice advertisedDevice) {
	    	// RSSI = Received signal strength indication
	    	if (advertisedDevice.getRSSI() > RSSI_THRESHOLD){
                addr_list.seen(advertisedDevice.getAddress());
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

   

	Serial.println("Starting scan...");
	BLEDevice::init(DEVICE_NAME);
	pBLEScan = BLEDevice::getScan(); //create new scan
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
	pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
	pBLEScan->setInterval(1000);
	pBLEScan->setWindow(1000);  // less or equal setInterval value
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
}

void loop() {
    if ( WiFi.status() != WL_CONNECTED){
        // If we lost WiFi connection, fuck it, just reset everything and try again.
        Serial.printf("Lost WiFi connection!");
        ESP.restart();
    }
    if (!mqtt_client.connected()){
        // But if its just mqtt, try to reconect
        Serial.printf("Lost MQTT connection! Trying to reconnect...");
        mqtt_reconnect();
    }
	BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME, false);
	Serial.print("Devices found: ");
	Serial.println(foundDevices.getCount());
    Serial.printf("list: %d\n%s", addr_list.size(), addr_list.toString().c_str());
    addr_list.tick();
	Serial.println("Scan done!");
	pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
}
