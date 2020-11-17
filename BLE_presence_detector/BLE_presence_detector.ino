// #include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Arduino.h>
// #include <WiFi.h>

// #include "0util.ino"
// #include <Streaming.h>

#define SCAN_TIME 1 // Time between each scan/tick in seconds
#define SEEN_TICKS 10 // How many ticks until a device is considered gone
#define RSSI_THRESHOLD -75 // Signals below this will be filtered
#define MAX_LEN 100 // Maximum amount of tracked MAC addresses
BLEScan* pBLEScan;


// #define SSID "NOME-DA-REDE-WIFI";
// #define PASSWORD  "SENHA-DA-REDE-WIFI";
// #define MQTTSERVER "iot.eclipse.org";
// #define MQTTPORT 1883;
// #define MQTTUSER "abcdefg";
// #define MQTTPASSWORD "123456";

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
                if(!addr_storage[i].last_seen){
                    Serial.printf("%s remove\n", toString(entry).c_str());
                    continue;
                }
                addr_storage[i].last_seen--;
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
	Serial.println("Scanning...");

	BLEDevice::init("");
	pBLEScan = BLEDevice::getScan(); //create new scan
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
	pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
	pBLEScan->setInterval(100);
	pBLEScan->setWindow(99);  // less or equal setInterval value
}

void loop() {
	// put your main code here, to run repeatedly:
	BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME, false);
	Serial.print("Devices found: ");
	Serial.println(foundDevices.getCount());
    Serial.printf("list: %d\n%s", addr_list.size(), addr_list.toString().c_str());
    addr_list.tick();
	Serial.println("Scan done!");
	pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
	delay(2000);
}
