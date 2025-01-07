#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_MitsubishiHeavy.h>
#include "DHT.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Arduino_APDS9960.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pin and Wi-Fi settings
#define DHTPIN1 25
#define DHTPIN2 26
#define DHTTYPE DHT22
#define WIFI_SSID "FlebinHome"
#define WIFI_PASSWORD "9820089048"
#define API_KEY "AIzaSyCZsHJnSoTnZQkTK_ia2ZBD-ZEoJW09thM"
#define DATABASE_URL "smartenergycontrolsystem-default-rtdb.firebaseio.com"
#define USER_EMAIL "benjaminsebastian4db@gmail.com"
#define USER_PASSWORD "database4benjamin"

// DHT sensors
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

const uint16_t kIrLed = 14;
IRMitsubishiHeavy152Ac ac(kIrLed);

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
int acTemp =0;
int pp=0;
bool updatedToOne = false; 
bool updatedToZero = false;
void setup() {
// Serial.println("Turning ON AC...");

    ac.begin();
    Serial.begin(115200);
    delay(200);

    dht1.begin();
    dht2.begin();
    connectWiFi();
    initFirebase();
    fetchInitialSettings();
    resetFirebaseNodes();
    // ac.on();  
    // ac.setFan(1);
    // ac.setMode(kDaikinCool);  
    // ac.setSwingVertical(false); 
    // ac.setSwingHorizontal(false); 

    // Initialize APDS-9960 sensor
    Serial.println("Initializing APDS-9960...");
    if (!APDS.begin()) {
        Serial.println("APDS-9960 initialization failed! Check connections.");
        while (1);
    }
    Serial.println("APDS-9960 initialized successfully.");
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("  Smart Energy ");
    lcd.setCursor(0,1);
    lcd.print(" Control System");

    delay(2000);
    lcd.clear();
}

void loop() {
  
    if (APDS.gestureAvailable()) {
      Serial.println(pp++);
        int gesture = APDS.readGesture();

        switch (gesture) {
            case GESTURE_UP:
                Serial.println("Gesture: UP");
                currentCount++;
                break;
            case GESTURE_DOWN:
                Serial.println("Gesture: DOWN");
                currentCount--;
                break;
            default:
                Serial.println("Gesture: UNKNOWN");
                break;
        }

        if (currentCount < 0) {currentCount = 0;}
        Firebase.RTDB.setInt(&fbdo, "/Data/Count", currentCount);
        Serial.println("Count : "+String(currentCount));
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
        ac.send();
        
        
        }
 #if SEND_MITSUBISHI_AC
  ac.send();
#endif 
        updateFirebaseData(temp1, temp2, humidity1, humidity2, indoorTemp, outdoorTemp, acTemp);
    }
   if (currentCount > 0 && !updatedToOne) {

  ac.setPower(true);  // Turn it on.
  ac.setFan(kMitsubishiHeavy152FanMed);  // Medium Fan
  ac.setMode(kMitsubishiHeavyCool);  // Cool mode
  ac.setTemp(acTemp);  // Celsius
  ac.setSwingVertical(kMitsubishiHeavy152SwingVAuto);  // Swing vertically
  ac.setSwingHorizontal(kMitsubishiHeavy152SwingHMiddle);  // Swing Horizontally
  ac.send();

    updatedToOne = true;  
    updatedToZero = false; 
    Serial.println("AC ON !");
Firebase.RTDB.setString(&fbdo, "/Data/SystemSTS", "Active");

  }

  if (currentCount == 0 && !updatedToZero) {
   ac.off();
  
  ac.send();

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

  }

}

void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to Wi-Fi...");
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

    Serial.printf("Fetched Settings: MinTemp = %.2f, MaxTemp = %.2f, MaxCapacity = %d\n", minTemp, maxTemp, maxCapacity);
}

void resetFirebaseNodes() {
    if(!Firebase.RTDB.setDouble(&fbdo, "/Data/Temperature-1", 0)||!Firebase.RTDB.setDouble(&fbdo, "/Data/Temperature-2", 0)||!Firebase.RTDB.setDouble(&fbdo, "/Data/Humidity-1", 0)||!Firebase.RTDB.setDouble(&fbdo, "/Data/Humidity-2", 0)||!Firebase.RTDB.setDouble(&fbdo, "/Data/ACTemperature", 0)){
      Serial.println("Error occured in firebase");
    }
}

void updateFirebaseData(double temp1, double temp2, double humidity1, double humidity2, double indoorTemp, double outdoorTemp, double acTemp) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Count:");
lcd.print(currentCount);
lcd.print(" AC:");
lcd.print(acTemp);
    lcd.setCursor(0,1);
    lcd.print("In:");
lcd.print(temp1);
    lcd.setCursor(7,1);
lcd.print(" Out:");
lcd.print(temp2);
   
Firebase.RTDB.setString(&fbdo, "/Data/Temperature-1", String(temp1, 1) );
Firebase.RTDB.setString(&fbdo, "/Data/Temperature-2", String(temp2, 1) );

Firebase.RTDB.setString(&fbdo, "/Data/Humidity-1", String(humidity1, 1));
Firebase.RTDB.setString(&fbdo, "/Data/Humidity-2", String(humidity2, 1));

Firebase.RTDB.setString(&fbdo, "/Data/ACTemperature", String(acTemp, 1) );
Firebase.RTDB.setInt(&fbdo, "/Data/Count", currentCount);

   


}

double mapValue(double value, double minOld, double maxOld, double minNew, double maxNew) {
    return minNew + ((value - minOld) / (maxOld - minOld)) * (maxNew - minNew);
}

int calculateAcTemp(double insideTemp, double outsideTemp, int capacity, int currentCount, double minTemp, double maxTemp) {
    double adjustedInsideTemp = insideTemp - 5;
    double mappedVal = mapValue(currentCount, 0, capacity, maxTemp, minTemp);
    double avgTemp = (adjustedInsideTemp + outsideTemp) / 2;
    double setAcTemp = (mappedVal + avgTemp) / 2;
    if(setAcTemp<=minTemp){
      setAcTemp=minTemp;
    }
    else if(setAcTemp>=maxTemp){
      setAcTemp=maxTemp;
    }
    Serial.printf("Calculated AC Temperature: %.2f\n", setAcTemp);
    return static_cast<int>(setAcTemp);
}

