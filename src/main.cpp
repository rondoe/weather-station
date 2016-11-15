
#include <OneWire.h>
#include <DallasTemperature.h>

#include <Adafruit_BMP085.h>
#include <Wire.h>

#include <PubSubClient.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
#define RAIN_SENSOR 14
#define WIND_SENSOR 13

#define VERSION 1.0

// Time to sleep (iconst char* ssid = "........";
const int sleepTimeS = 10;

const char* ssid = "........";
const char* password = "........");

const char* mqtt_server = "mqtt.home.rondoe.com";

WiFiClient espClient;
PubSubClient client(espClient);

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);



Adafruit_BMP085 bmp;

//at the beginning of sketch
ADC_MODE(ADC_VCC); //vcc read


/**
 * Blink
 *
 * Turns on an LED on for one second,
 * then off for one second, repeatedly.
 */
#include "Arduino.h"


volatile int rpmcount = 0;

void rpm_vent() {
  Serial.println("xxx");
  rpmcount++;
}


void readSensors() {

  // READ sensors

  digitalWrite(LED_BUILTIN, HIGH);
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");

  Serial.print("Temperature for the device 1 (index 0) is: ");
  Serial.println(sensors.getTempCByIndex(0));
  client.publish("myhome/", "hello world");


  Serial.print("Temperature = ");
  Serial.print(bmp.readTemperature());
  Serial.println(" *C");

  Serial.print("Pressure = ");
  float pressure =bmp.readPressure()/100;
  Serial.print(pressure);
  Serial.println(" hPa");

  // Calculate altitude assuming 'standard' barometric
  // pressure of 1013.25 millibar = 101325 Pascal
  Serial.print("Altitude = ");
  Serial.print(bmp.readAltitude());
  Serial.println(" meters");

  // you can get a more precise measurement of altitude
  // if you know the current sea level pressure which will
  // vary with weather and such. If it is 1015 millibars
  // that is equal to 101500 Pascals.
  Serial.print("Real altitude = ");
  Serial.print(bmp.readAltitude(101500));
  Serial.println(" meters");



  // read digital sensor inverted
  int raining = !digitalRead(RAIN_SENSOR);
  Serial.print("Rain = ");
  Serial.println(raining);

  // loop for 1 second to determine wind speed
  // v [km/h] = 1.761 / (1 + rps) + 3.013 * rps
  int lastmillis = millis();
  rpmcount = 0 ;
  // pinMode(RAIN_SENSOR, INPUT_PULLUP);
  while(millis()-lastmillis<1000) {}
  Serial.print("Wind = ");
  Serial.println(rpmcount);
  // v [km/h] = 1.761 / (1 + rps) + 3.013 * rps
  float speed = 1.761 / (1+rpmcount) + 3.013 * rpmcount;
  speed = (rpmcount+2)/3;
  Serial.print("Speed = ");
  Serial.print(speed);
  Serial.print("m/s");
  Serial.print(" ");
  Serial.print(speed * 3.6);
  Serial.println("km/h");



  //get voltage
  float vdd = ESP.getVcc() / 1000.0;

  digitalWrite(LED_BUILTIN, LOW);
}



void setup()
{
  // initialize LED digital pin as an output.
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  attachInterrupt(WIND_SENSOR, rpm_vent, FALLING);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    client.setServer(mqtt_server, 1883);


  // Start up the library
sensors.begin();
if (!bmp.begin()) {
Serial.println("Could not find a valid BMP085 sensor, check wiring!");
}

////////////////////////////////////



// need to do all the stuff in setup, as wakeup calls setup again and not loop
// goto sleep
// Serial.println("ESP8266 in sleep mode");
// ESP.deepSleep(sleepTimeS * 1000000);

}

void loop()
{

    readSensors();
    delay(5000);


}
