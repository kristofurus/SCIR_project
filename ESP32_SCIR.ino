#include <Arduino.h>
#include <Wire.h>

// SHT31
// https://github.com/adafruit/Adafruit_SHT31
#include "Adafruit_SHT31.h"

// BMP280
// https://github.com/adafruit/Adafruit_BMP280_Library
// 23.10.2022 -> BMP280 stopped working
// 19.11.2022 -> changed communication type from I2C to SPI. BP280 started working
#include <Adafruit_BMP280.h>

// DHT11 & DHT22
// https://github.com/adafruit/DHT-sensor-library
#include "DHT.h"

// DS18B20
#include <OneWire.h>
// https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <DallasTemperature.h>

// ThingSpeak / Wifi
#include <WiFi.h>
#include "secrets.h"
// https://github.com/mathworks/thingspeak-arduino
#include "ThingSpeak.h"

// ------------------------------------------- USED PINS -------------------------------------------//
// Communication pins
// GPIO23 - MOSI  (SPI)
// GPIO22 - SCL   (I2C)
// GPIO21 - SDA   (I2C)
// GPIO19 - MISO  (SPI)
// GPIO18 - SCK   (SPI)
// GPIO4  - One wire bus
#define ONE_WIRE_BUS 4

// ESP32 status pins
// GPIO2  - LED   (SEND STATUS)
#define LED_PIN 2
// GPIO34 - V_bat
#define BAT_PIN 34

// Sensors pins
// GPIO5  - BMP_CS
# define BMP_CS 5
// GPIO32 - DHT11
# define DHT11_PIN 32
// GPIO33 - DHT22
# define DHT22_PIN 33

// ------------------------------------------- SENSORS DECLARATION -------------------------------------------//

Adafruit_SHT31 sht31 = Adafruit_SHT31();

Adafruit_BMP280 bmp(BMP_CS);

DHT dht11(DHT11_PIN, DHT11);

DHT dht22(DHT22_PIN, DHT22);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dsTemp(&oneWire);

// ------------------------------------------- GLOBAL PARAMETERS -------------------------------------------//

String ThingSpeakStatus = "";

const unsigned int delayTime = 20000;
const unsigned int batteryMeasureLimit = 15;

unsigned int batteryMeasureCounter = 0;
int bat_values[batteryMeasureLimit];

float bat_value = 0;
float bat_voltage = 0.0f;

// ------------------------------------------- THING SPEAK / WIFI -------------------------------------------//
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
WiFiClient  client;

unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;

void setup() {

  // init wifi and thingspeak
  WiFi.mode(WIFI_STA);   
  ThingSpeak.begin(client);

  // connect to wifi
  if(WiFi.status() != WL_CONNECTED){
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, pass); 
      delay(5000);     
    } 
  }
  
  // initialize sensors
  if (!bmp.begin()) {
    ThingSpeakStatus = "Error with BMP280";
    ThingSpeak.setStatus(ThingSpeakStatus);
    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    delay(60000);
    ESP.restart();
  }
  
  if (!sht31.begin(0x44)) {
    ThingSpeakStatus = "Error with SHT31";
    ThingSpeak.setStatus(ThingSpeakStatus);
    ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    delay(60000);
    ESP.restart();
  }

  dht11.begin();
  dht22.begin();
  dsTemp.begin();

  // declaration LED pin as output
  pinMode(LED_PIN,OUTPUT);

}

void loop() {

  // reconnect to wifi if needed
  if(WiFi.status() != WL_CONNECTED){
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, pass);
      delay(500);     
    } 
  }

  // read values from sensors

  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  float t2 = bmp.readTemperature();

  float h11 = dht11.readHumidity();
  float t11 = dht11.readTemperature();

  float h22 = dht22.readHumidity();
  float t22 = dht22.readTemperature();

  // temperature 85*C is reset temperature value. To read correct temp use 
  // requestTemperatures() method
  dsTemp.requestTemperatures();
  float tDS = dsTemp.getTempCByIndex(0);

  // send values to cloud

  // SHT31
  ThingSpeak.setField(1, t);
  ThingSpeak.setField(2, h);

  // BMP280
  ThingSpeak.setField(3, t2);

  // DHT11
  ThingSpeak.setField(4, t11);
  ThingSpeak.setField(5, h11);

  // DHT22
  ThingSpeak.setField(6, t22);
  ThingSpeak.setField(7, h22);

  // DS18B20
  ThingSpeak.setField(8, tDS);

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  // led signalization for sending sensor data
  if(x == 200) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
  } else {
    digitalWrite(LED_PIN, HIGH);
  }

  if (batteryMeasureCounter < batteryMeasureLimit) {
    bat_values[batteryMeasureCounter] = analogRead(BAT_PIN);
    batteryMeasureCounter++;
  } else {
    batteryMeasureCounter = 0;

    int sum = 0;
    for(int i = 0; i < batteryMeasureLimit; i++) {
      sum += bat_values[i]; 
    }
    
    bat_value = float(sum)/batteryMeasureLimit;
    bat_voltage = float(bat_value) / 4096 * 3.3 * 2;
    
    ThingSpeak.setField(1, bat_voltage);
    ThingSpeak.setField(2, bat_value);

    x = ThingSpeak.writeFields(SECRET_BATTERY_ID, SECRED_BATTERY_APIKEY);

    // led signalization for sending battery data
  }

  // wait
  delay(delayTime);

}
