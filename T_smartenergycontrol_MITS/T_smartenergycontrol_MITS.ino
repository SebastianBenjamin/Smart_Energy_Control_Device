#include <SoftwareSerial.h>
#include <NewPing.h>
#include "DHT.h"

// Pin Configuration
#define TRIG_PIN1 7    
#define ECHO_PIN1 6    
#define TRIG_PIN2 5    
#define ECHO_PIN2 4    
#define DHTPIN 2       
#define DHTTYPE DHT22
#define BT_RX 11      
#define BT_TX 10     

// Constants
#define MAX_DISTANCE 1000
#define DETECTION_DISTANCE 100
#define CROSS_TIME 3000     // Maximum time to cross both sensors
#define RESET_TIME 3000     // Time before resetting trigger state
#define UPDATE_INTERVAL 5000 // Send temp/humidity every 5 seconds

// Initialize objects
NewPing sensor1(TRIG_PIN1, ECHO_PIN1, MAX_DISTANCE);
NewPing sensor2(TRIG_PIN2, ECHO_PIN2, MAX_DISTANCE);
DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial bluetooth(BT_TX, BT_RX);

// Global variables
unsigned long sensor1TriggerTime = 0;
unsigned long sensor2TriggerTime = 0;
unsigned long lastUpdateTime = 0;
bool sensor1Active = false;
bool sensor2Active = false;

void setup() {
    Serial.begin(9600);
    bluetooth.begin(9600);
    dht.begin();
}

void sendData(const String& movement) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if (!isnan(t) && !isnan(h)) {
        String data = String(t, 1) + "," + String(h, 1) + "," + movement;
        // Serial.println(data);
        bluetooth.println(data);
        delay(500);
    }
}

void sendSensorUpdate() {
    unsigned long currentTime = millis();
    if (currentTime - lastUpdateTime >= UPDATE_INTERVAL) {
        sendData("NONE");
        lastUpdateTime = currentTime;
    }
}

void loop() {
    int dist1 = sensor1.ping_cm();
    int dist2 = sensor2.ping_cm();
    unsigned long currentTime = millis();
    
    // Prevent false readings
    if (dist1 == 0) dist1 = MAX_DISTANCE;
    if (dist2 == 0) dist2 = MAX_DISTANCE;
    
    // Check sensor 1
    if (dist1 < DETECTION_DISTANCE && !sensor1Active) {
        sensor1Active = true;
        sensor1TriggerTime = currentTime;
    }
    
    // Check sensor 2
    if (dist2 < DETECTION_DISTANCE && !sensor2Active) {
        sensor2Active = true;
        sensor2TriggerTime = currentTime;
    }
    
    // Process triggers
    if (sensor1Active && sensor2Active) {
        if (sensor1TriggerTime < sensor2TriggerTime && 
            (sensor2TriggerTime - sensor1TriggerTime) < CROSS_TIME) {
            sendData("OUT");
        }
        else if (sensor2TriggerTime < sensor1TriggerTime && 
                 (sensor1TriggerTime - sensor2TriggerTime) < CROSS_TIME) {
            sendData("IN");
        }
        
        // Reset triggers
        sensor1Active = false;
        sensor2Active = false;
    }
    
    // Reset if single trigger times out
    if (sensor1Active && (currentTime - sensor1TriggerTime > RESET_TIME)) {
        sensor1Active = false;
    }
    if (sensor2Active && (currentTime - sensor2TriggerTime > RESET_TIME)) {
        sensor2Active = false;
    }
  
}