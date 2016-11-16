
#include <FS.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

#include <Adafruit_BMP085.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266httpUpdate.h>

#include <ArduinoJson.h>
#include "Arduino.h"


///////////////////////////////////////////////


// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2
#define RAIN_SENSOR 14
#define WIND_SENSOR 13

#define VERSION 1.0

// Time to sleep (iconst char* ssid = "........";
const int sleepTimeS = 10;


//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "1883";
char mqtt_prefix[40] = "controller";
char mqtt_node[6] = "";

#define EEPROM_OFFSET 256
struct config_t
{
        bool connected = false;
        char ssid[32];
        char password[32];
} configuration;



WiFiClient espClient;
PubSubClient client(espClient);

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);


Adafruit_BMP085 bmp;

//at the beginning of sketch
ADC_MODE(ADC_VCC); //vcc read



bool configOk = true;
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
        rpmcount = 0;
        int lastmillis = millis();
        // pinMode(RAIN_SENSOR, INPUT_PULLUP);
        while(millis()-lastmillis<5000) { delay(1); }
        // divide by 2, we checked 2 seconds
        float rpm = rpmcount/5;
        Serial.print("Wind = ");
        Serial.println(rpm);
        float speed = 0;
        if(rpm>0) {
                // v [km/h] = 1.761 / (1 + rps) + 3.013 * rps
                speed = 1.761 / (1+rpm) + 3.013 * rpm;
                speed = (rpm+2)/3;
        }
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


void setupWifi() {


        //WiFiManager
        //Local intialization. Once its business is done, there is no need to keep it around
        WiFiManager wifiManager;


        if(!configOk) {
                wifiManager.resetSettings();
        }

        // The extra parameters to be configured (can be either global or just in the setup)
        // After connecting, parameter.getValue() will get you the configured value
        // id/name placeholder/prompt default length
        WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
        WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
        WiFiManagerParameter custom_mqtt_prefix("prefix", "mqtt prefix", mqtt_prefix, 40);
        WiFiManagerParameter custom_mqtt_node("node", "node", mqtt_node, 6);

        wifiManager.addParameter(&custom_mqtt_server);
        wifiManager.addParameter(&custom_mqtt_port);
        wifiManager.addParameter(&custom_mqtt_prefix);
        wifiManager.addParameter(&custom_mqtt_node);

        //exit after config instead of connecting
        wifiManager.setBreakAfterConfig(true);


        //tries to connect to last known settings
        //if it does not connect it starts an access point with the specified name
        //here  "AutoConnectAP" with password "password"
        //and goes into a blocking loop awaiting configuration
        if (!wifiManager.autoConnect("WeatherStation", "password")) {
                Serial.println("failed to connect, we should reset as see if it connects");
                delay(3000);
                ESP.reset();
                delay(5000);
        }


        //read updated parameters
        strcpy(mqtt_server, custom_mqtt_server.getValue());
        strcpy(mqtt_port, custom_mqtt_port.getValue());
        strcpy(mqtt_prefix, custom_mqtt_prefix.getValue());
        strcpy(mqtt_node, custom_mqtt_node.getValue());


        Serial.println("saving config");
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.createObject();
        json["mqtt_server"] = mqtt_server;
        json["mqtt_port"] = mqtt_port;
        json["mqtt_prefix"] = mqtt_prefix;
        json["mqtt_node"] = mqtt_node;

        File configFile = SPIFFS.open("/config.json", "w");
        if (!configFile) {
                Serial.println("failed to open config file for writing");
        }

        json.printTo(Serial);
        json.printTo(configFile);
        configFile.close();
//end save


//if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");


        Serial.println("local ip");
        Serial.println(WiFi.localIP());


        // OTA updated

        t_httpUpdate_return ret = ESPhttpUpdate.update("ota.rondoe.com", 80, "/esp/arduino", "1.0");
        switch(ret) {
        case HTTP_UPDATE_FAILED:
                Serial.println("[update] Update failed.");
                break;
        case HTTP_UPDATE_NO_UPDATES:
                Serial.println("[update] Update no Update.");
                break;
        case HTTP_UPDATE_OK:
                Serial.println("[update] Update ok."); // may not called we reboot the ESP
                break;
        }

}

void readConfig() {
        if (SPIFFS.begin()) {
                Serial.println("mounted file system");
                if (SPIFFS.exists("/config.json")) {
                        //file exists, reading and loading
                        Serial.println("reading config file");
                        File configFile = SPIFFS.open("/config.json", "r");
                        if (configFile) {
                                Serial.println("opened config file");
                                size_t size = configFile.size();
                                // Allocate a buffer to store contents of the file.
                                std::unique_ptr<char[]> buf(new char[size]);

                                configFile.readBytes(buf.get(), size);
                                DynamicJsonBuffer jsonBuffer;
                                JsonObject& json = jsonBuffer.parseObject(buf.get());
                                json.printTo(Serial);
                                if (json.success()) {
                                        Serial.println("\nparsed json");

                                        strcpy(mqtt_server, json["mqtt_server"]);
                                        strcpy(mqtt_port, json["mqtt_port"]);
                                        if(json.containsKey("mqtt_prefix")) {
                                                strcpy(mqtt_prefix, json["mqtt_prefix"]);
                                        }
                                        else {
                                                configOk = false;
                                        }
                                        if(json.containsKey("mqtt_node")) {
                                                strcpy(mqtt_node, json["mqtt_node"]);
                                        } else {
                                                configOk = false;
                                        }
                                } else {
                                        Serial.println("failed to load json config");
                                }
                        }
                }
        } else {
                Serial.println("failed to mount FS");
        }
}


void setup()
{
        // initialize LED digital pin as an output.
        Serial.begin(115200);
        pinMode(LED_BUILTIN, OUTPUT);

        // read config
        readConfig();
        // wifi setup
        setupWifi();

        attachInterrupt(WIND_SENSOR, rpm_vent, FALLING);

        // Start up the library
        sensors.begin();
        if (!bmp.begin()) {
                Serial.println("Could not find a valid BMP085 sensor, check wiring!");
        }



}
////////////////////////////////////



// need to do all the stuff in setup, as wakeup calls setup again and not loop
// goto sleep
// Serial.println("ESP8266 in sleep mode");
// ESP.deepSleep(sleepTimeS * 1000000);

void loop()
{

        readSensors();
        delay(5000);


}
