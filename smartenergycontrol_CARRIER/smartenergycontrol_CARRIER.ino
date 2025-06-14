#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Carrier.h>
#include "DHT.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Arduino_APDS9960.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NewPing.h>
// Pin and Wi-Fi settings
#define TRIG_PIN1 5
#define ECHO_PIN1 4
#define TRIG_PIN2 18 
#define ECHO_PIN2 19
#define MAX_DISTANCE 1000     
#define THRESHOLD 50
 
#define DHTPIN1 25
#define DHTPIN2 26
#define DHTTYPE DHT22
#define WIFI_SSID "SmartEnergyControlSystem"
#define WIFI_PASSWORD "12345678"
#define API_KEY "AIzaSyCZsHJnSoTnZQkTK_ia2ZBD-ZEoJW09thM"
#define DATABASE_URL "smartenergycontrolsystem-default-rtdb.firebaseio.com"
#define USER_EMAIL "benjaminsebastian4db@gmail.com"
#define USER_PASSWORD "database4benjamin"

#define BUZZER_PIN 13
#define LED_PIN 12
#define WIFI_CHECK_INTERVAL 5000

NewPing sensor1(TRIG_PIN1, ECHO_PIN1, MAX_DISTANCE);
NewPing sensor2(TRIG_PIN2, ECHO_PIN2, MAX_DISTANCE);

// DHT sensors
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

const uint16_t kIrLed = 23;
IRCarrierAc64 ac(kIrLed);  // Changed to Carrier AC

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Global variables
int maxCapacity = 50;
double minTemp = .0;
double maxTemp = 24.0;
int lastCount = -1;
int currentCount = 0;
bool updatedToOne = false;
bool updatedToZero = false;
bool s1Triggered = false;
bool s2Triggered = false;
int acTemp = 0;
unsigned long lastWifiCheck = 0;
bool lastWifiState = false;  // Track previous WiFi state

void updateWiFiLED() {
    bool currentWifiState = (WiFi.status() == WL_CONNECTED);
    
    // Update LED state
    digitalWrite(LED_PIN, currentWifiState ? HIGH : LOW);
    
    // Only update LCD if WiFi state has changed
    if (currentWifiState != lastWifiState) {
        lcd.clear();
        if (currentWifiState) {
            lcd.setCursor(0, 0);
            lcd.print("WiFi ");
            lcd.setCursor(0, 1);
            lcd.print(" Connected !");
        } else {
            lcd.setCursor(0, 0);
            lcd.print("Continuing  ");
            lcd.setCursor(0, 1);
            lcd.print("without WiFi !");
        }
        delay(2000);
        lcd.clear();
        lastWifiState = currentWifiState;
        
        // Update display with current system status
        if (currentCount == 0) {
            lcd.setCursor(0, 0);
            lcd.print("Count:");
            lcd.print(currentCount);
            lcd.print(" AC:OFF");
            lcd.setCursor(0, 1);
            lcd.print("System Inactive");
        }
    }
}

void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Connecting to Wi-Fi...");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    bool currentWifiState = (WiFi.status() == WL_CONNECTED);
    lastWifiState = currentWifiState;
    
    lcd.clear();
    if (currentWifiState) {
        Serial.println("Wi-Fi connected.");
        lcd.setCursor(0, 0);
        lcd.print("WiFi ");
        lcd.setCursor(0, 1);
        lcd.print(" Connected !");
    } else {
        Serial.println("Wi-Fi connection failed, continuing without WiFi.");
        lcd.setCursor(0, 0);
        lcd.print("Continuing  ");
        lcd.setCursor(0, 1);
        lcd.print("without WiFi !");
    }
    delay(2000);
    lcd.clear();
}

void initFirebase() {
    if (WiFi.status() == WL_CONNECTED) {
        config.api_key = API_KEY;
        auth.user.email = USER_EMAIL;
        auth.user.password = USER_PASSWORD;
        config.database_url = DATABASE_URL;
        Firebase.begin(&config, &auth);
    }
}

void fetchInitialSettings() {
    if (WiFi.status() == WL_CONNECTED) {
        if (Firebase.RTDB.getDouble(&fbdo, "/Settings/MinTemp")) {
            minTemp = fbdo.to<double>();
        }
        if (Firebase.RTDB.getDouble(&fbdo, "/Settings/MaxTemp")) {
            maxTemp = fbdo.to<double>();
        }
        if (Firebase.RTDB.getInt(&fbdo, "/Settings/MaxCapacity")) {
            maxCapacity = fbdo.to<int>();
        }
        if (Firebase.RTDB.getInt(&fbdo, "/Data/Count")) {
            currentCount = fbdo.to<int>();
        }
        Serial.printf("Fetched Settings: MinTemp = %.2f, MaxTemp = %.2f, MaxCapacity = %d Count = %d \n", 
                     minTemp, maxTemp, maxCapacity, currentCount);
    }
}

void resetFirebaseNodes() {
    if (WiFi.status() == WL_CONNECTED) {
        Firebase.RTDB.setDouble(&fbdo, "/Data/Temperature-1", 0);
        Firebase.RTDB.setDouble(&fbdo, "/Data/Temperature-2", 0);
        Firebase.RTDB.setDouble(&fbdo, "/Data/Humidity-1", 0);
        Firebase.RTDB.setDouble(&fbdo, "/Data/Humidity-2", 0);
        Firebase.RTDB.setDouble(&fbdo, "/Data/ACTemperature", 0);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    
    dht1.begin();
    dht2.begin();
    
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("  Smart Energy ");
    lcd.setCursor(0, 1);
    lcd.print(" Control System");
    delay(2000);
    lcd.clear();
    
    connectWiFi();
    initFirebase();
    fetchInitialSettings();
    resetFirebaseNodes();

    pinMode(kIrLed, OUTPUT);
    digitalWrite(kIrLed, HIGH);
}

void resetStates() {
    s1Triggered = false;
    s2Triggered = false;
}

void singleBeep() {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
}

void doubleBeep() {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
}

double mapValue(double value, double minOld, double maxOld, double minNew, double maxNew) {
    return minNew + ((value - minOld) / (maxOld - minOld)) * (maxNew - minNew);
}

int calculateAcTemp(double insideTemp, double outsideTemp, int capacity, int currentCount, 
                   double minTemp, double maxTemp) {
    double adjustedInsideTemp = insideTemp - 5;
    double mappedVal = mapValue(currentCount, 0, capacity, maxTemp, minTemp);
    double avgTemp = (adjustedInsideTemp + outsideTemp) / 2;
    double setAcTemp = (mappedVal + avgTemp) / 2;
    
    if (setAcTemp <= minTemp) {
        setAcTemp = minTemp;
    } else if (setAcTemp >= maxTemp) {
        setAcTemp = maxTemp;
    }
    
    Serial.printf("Calculated AC Temperature: %.2f\n", setAcTemp);
    return static_cast<int>(setAcTemp);
}

void updateFirebaseData(double temp1, double temp2, double humidity1, double humidity2, 
                       double indoorTemp, double outdoorTemp, double acTemp) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Count:");
    lcd.print(currentCount);
    lcd.print(" AC:");
    lcd.print(acTemp);
    lcd.setCursor(0, 1);
    lcd.print("In:");
    lcd.print(temp1);
    lcd.setCursor(7, 1);
    lcd.print(" Out:");
    lcd.print(temp2);

    if (WiFi.status() == WL_CONNECTED) {
        Firebase.RTDB.setString(&fbdo, "/Data/Temperature-1", String(temp1, 1));
        Firebase.RTDB.setString(&fbdo, "/Data/Temperature-2", String(temp2, 1));
        Firebase.RTDB.setString(&fbdo, "/Data/Humidity-1", String(humidity1, 1));
        Firebase.RTDB.setString(&fbdo, "/Data/Humidity-2", String(humidity2, 1));
        Firebase.RTDB.setString(&fbdo, "/Data/ACTemperature", String(acTemp, 1));
        Firebase.RTDB.setInt(&fbdo, "/Data/Count", currentCount);
    }
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
        updateWiFiLED();
        
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
        
        lastWifiCheck = currentMillis;
    }

    double temp1 = dht1.readTemperature();
    double humidity1 = dht1.readHumidity();
    double temp2 = dht2.readTemperature();
    double humidity2 = dht2.readHumidity();
    
    int distance1 = sensor1.ping_cm();
    int distance2 = sensor2.ping_cm();

    if (distance1 < THRESHOLD) s1Triggered = true;
    if (distance2 < THRESHOLD) s2Triggered = true;

    if (s1Triggered) {
        delay(10);
        if (sensor2.ping_cm() < THRESHOLD) {
            lcd.clear();
            lcd.setCursor(0, 1);
            lcd.print("   Person IN");
            Serial.println("ENTERED");
            currentCount++;
            resetStates();
            singleBeep();
        }
    }

    if (s2Triggered) {
        delay(10);
        if (sensor1.ping_cm() < THRESHOLD) {
            lcd.clear();
            lcd.setCursor(0, 1);
            lcd.print("   Person OUT");
            Serial.println("EXITED");
            if (currentCount > 0) currentCount--;
            resetStates();
            doubleBeep();
        }
    }

    if (currentCount != lastCount) {
        lastCount = currentCount;
        double indoorTemp = temp1;
        double outdoorTemp = temp2;

        acTemp = calculateAcTemp(indoorTemp, outdoorTemp, maxCapacity, currentCount, minTemp, maxTemp);
        if (updatedToOne) {
            Serial.println(" Setting AC Temperature to :" + String(acTemp));
            ac.setTemp(acTemp);
            ac.send();
        }
#if SEND_CARRIER_AC64
        ac.send();
#endif  
        updateFirebaseData(temp1, temp2, humidity1, humidity2, indoorTemp, outdoorTemp, acTemp);
    }

    if (currentCount > 0 && !updatedToOne) {
          ac.on();
        ac.setFan(kCarrierAc64FanAuto);
        ac.setMode(kCarrierAc64Cool);
        ac.setTemp(acTemp);
        ac.send();
        
#if SEND_CARRIER_AC64
        ac.send();
#endif 
        updatedToOne = true;
        updatedToZero = false;
        Serial.println("AC ON !");
        if (WiFi.status() == WL_CONNECTED) {
            Firebase.RTDB.setString(&fbdo, "/Data/SystemSTS", "Active");
        }
    }

    if (currentCount == 0 && !updatedToZero) {
        ac.off();
        ac.send();
#if SEND_CARRIER_AC64
        ac.send();
#endif 
        updatedToZero = true;
        updatedToOne = false;
        Serial.println("AC OFF !");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Count:");
        lcd.print(currentCount);
        lcd.print(" AC:OFF");
        lcd.setCursor(0, 1);
        lcd.print("System Inactive");
        
        if (WiFi.status() == WL_CONNECTED) {
            Firebase.RTDB.setString(&fbdo, "/Data/SystemSTS", "In Active");
        }
        
        for (int i = 0; i < 2; i++) {
            doubleBeep();
        }
    }
}