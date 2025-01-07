#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Mitsubishi.h>
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

#define BUZZER_PIN 13  // Define Buzzer Pin
#define LED_PIN 12  

NewPing sensor1(TRIG_PIN1, ECHO_PIN1, MAX_DISTANCE);
NewPing sensor2(TRIG_PIN2, ECHO_PIN2, MAX_DISTANCE);

// DHT sensors
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

const uint16_t kIrLed = 23;
IRMitsubishiAC ac(kIrLed);


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
int acTemp=0;

void setup() {
    Serial.begin(115200);

    pinMode(BUZZER_PIN, OUTPUT);  // Set the buzzer pin as an output
    pinMode(LED_PIN, OUTPUT);  // Set the buzzer pin as an output

    dht1.begin();
    dht2.begin();
    connectWiFi();
    initFirebase();
    fetchInitialSettings();
    resetFirebaseNodes();

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("  Smart Energy ");
    lcd.setCursor(0, 1);
    lcd.print(" Control System");
    pinMode(kIrLed,OUTPUT);
    digitalWrite(kIrLed,HIGH);
    delay(2000);
    lcd.clear();
}

void loop() {
  double temp1 = dht1.readTemperature();
        double humidity1 = dht1.readHumidity();
        double temp2 = dht2.readTemperature();
        double humidity2 = dht2.readHumidity();
    // Ultrasonic Sensor Logic
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
            singleBeep();  // Single beep when count increases
            
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
            doubleBeep();  // Double beep when count decreases
            
        }
    }

    if (currentCount != lastCount) {
        lastCount = currentCount;
       double temp1 = dht1.readTemperature();
      double humidity1 = dht1.readHumidity();
      double temp2 = dht2.readTemperature();
      double humidity2 = dht2.readHumidity();
        double indoorTemp = temp1;
        double outdoorTemp = temp2;

         acTemp = calculateAcTemp(indoorTemp, outdoorTemp, maxCapacity, currentCount, minTemp, maxTemp);
       if(updatedToOne){
        Serial.println(" Setting AC Temperature to :"+String(acTemp));
        ac.setTemp(acTemp);
        ac.send();}
 #if SEND_MITSUBISHI_AC
  ac.send();
#endif  
        updateFirebaseData(temp1, temp2, humidity1, humidity2, indoorTemp, outdoorTemp, acTemp);
    }
   if (currentCount > 0 && !updatedToOne) {
  ac.on();
  ac.setFan(1);
  ac.setMode(kMitsubishiAcCool);
  ac.setTemp(acTemp);
  ac.setVane(kMitsubishiAcVaneAuto);
    #if SEND_MITSUBISHI_AC
  ac.send();
#endif 
    updatedToOne = true;  
    updatedToZero = false; 
    Serial.println("AC ON !");
Firebase.RTDB.setString(&fbdo, "/Data/SystemSTS", "Active");
  }

  if (currentCount == 0 && !updatedToZero) {
   ac.off();
   #if SEND_MITSUBISHI_AC
  ac.send();
#endif 
    updatedToZero = true; 
    updatedToOne = false; 
    Serial.println("AC OFF !");
     lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Count:");
lcd.print(currentCount);
lcd.print(" AC:OFF");
    lcd.setCursor(0,1);
    lcd.print("System Inactive");
Firebase.RTDB.setString(&fbdo, "/Data/SystemSTS", "In Active");
 digitalWrite(BUZZER_PIN, HIGH);
    delay(200);  // 200 ms beep duration
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);  // Pause between beeps
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);  // Second beep
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);  // Pause after second beep
     digitalWrite(BUZZER_PIN, HIGH);
    delay(200);  // 200 ms beep duration
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);  // Pause between beeps
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);  // Second beep
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);  // Pause after second beep

  }

}


void resetStates() {
    s1Triggered = false;
    s2Triggered = false;
}

void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        Serial.println("Connecting to Wi-Fi...");
    while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(LED_PIN,HIGH);
        delay(500);
      digitalWrite(LED_PIN,LOW);
        delay(500);
        Serial.println(".");
    }
    if(WiFi.status()==WL_CONNECTED){
      digitalWrite(LED_PIN,HIGH);

    }
    Serial.println("Wi-Fi connected.");
}

void initFirebase() {
    config.api_key = API_KEY;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    config.database_url = DATABASE_URL;

    Firebase.begin(&config, &auth);
}

void fetchInitialSettings() {
    if (Firebase.RTDB.getDouble(&fbdo, "/Settings/MinTemp")) {
        minTemp = fbdo.to<double>();
    } else {
        Serial.println("Failed to fetch MinTemp. Using default.");
    }

    if (Firebase.RTDB.getDouble(&fbdo, "/Settings/MaxTemp")) {
        maxTemp = fbdo.to<double>();
    } else {
        Serial.println("Failed to fetch MaxTemp. Using default.");
    }

    if (Firebase.RTDB.getInt(&fbdo, "/Settings/MaxCapacity")) {
        maxCapacity = fbdo.to<int>();
    } else {
        Serial.println("Failed to fetch MaxCapacity. Using default.");
    }
    if (Firebase.RTDB.getInt(&fbdo, "/Data/Count")) {
        currentCount = fbdo.to<int>();
    } else {
        Serial.println("Failed to fetch Count. Using default.");
    }

    Serial.printf("Fetched Settings: MinTemp = %.2f, MaxTemp = %.2f, MaxCapacity = %d Count = %d \n", minTemp, maxTemp, maxCapacity,currentCount);
}

void resetFirebaseNodes() {
    if (!Firebase.RTDB.setDouble(&fbdo, "/Data/Temperature-1", 0) || !Firebase.RTDB.setDouble(&fbdo, "/Data/Temperature-2", 0) ||
        !Firebase.RTDB.setDouble(&fbdo, "/Data/Humidity-1", 0) || !Firebase.RTDB.setDouble(&fbdo, "/Data/Humidity-2", 0) ||
        !Firebase.RTDB.setDouble(&fbdo, "/Data/ACTemperature", 0)) {
        Serial.println("Error occurred in Firebase");
    }
}

void updateFirebaseData(double temp1, double temp2, double humidity1, double humidity2, double indoorTemp, double outdoorTemp, double acTemp) {
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

    Firebase.RTDB.setString(&fbdo, "/Data/Temperature-1", String(temp1, 1));
    Firebase.RTDB.setString(&fbdo, "/Data/Temperature-2", String(temp2, 1));
    Firebase.RTDB.setString(&fbdo, "/Data/Humidity-1", String(humidity1, 1));
    Firebase.RTDB.setString(&fbdo, "/Data/Humidity-2", String(humidity2, 1));
    Firebase.RTDB.setString(&fbdo, "/Data/ACTemperature", String(acTemp, 1));
    Firebase.RTDB.setInt(&fbdo, "/Data/Count", currentCount);
}

void singleBeep() {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);  // 200 ms beep duration
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);  // Pause between beeps
}

void doubleBeep() {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);  // 200 ms beep duration
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);  // Pause between beeps
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);  // Second beep
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);  // Pause after second beep
}

double mapValue(double value, double minOld, double maxOld, double minNew, double maxNew) {
    return minNew + ((value - minOld) / (maxOld - minOld)) * (maxNew - minNew);
}

int calculateAcTemp(double insideTemp, double outsideTemp, int capacity, int currentCount, double minTemp, double maxTemp) {
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
