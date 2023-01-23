#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h> 
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include "cert.h"
#include "freertos/FreeRTOS.h"
#include <Adafruit_Sensor.h>
#include "DHT.h"

WebServer server(80);

int status = WL_IDLE_STATUS;
int incomingByte;

String FirmwareVer = {
    "0.02"
};

#define URL_fw_Version "https://raw.githubusercontent.com/d3m0nz/tantrum/main/bin_version.txt"
#define URL_fw_Bin "https://raw.githubusercontent.com/d3m0nz/tantrum/main/fw.bin"


const int red_pin = 5;   
const int green_pin = 18; 
const int blue_pin = 19; 

// Setting PWM frequency, channels and bit resolution
const int frequency = 5000;
const int redChannel = 0;
const int greenChannel = 1;
const int blueChannel = 2;
const int resolution = 8;



#define DHTPIN 4  
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

StaticJsonDocument<250> jsonDocument;
char buffer[250];

float temperature;
float humidity;
float pressure;
 
void setup_routing() {   
  dht.begin();  
  server.on("/temperature", getTemperature);     
  server.on("/pressure", getPressure);     
  server.on("/humidity", getHumidity);     
  server.on("/data", getData);     
  server.on("/led", HTTP_POST, rgbLEDs);    
  server.on("/update", firmwareUpdate);        
  server.begin();    
}
 
void create_json(char *tag, float value, char *unit) {  
  jsonDocument.clear();  
  jsonDocument["type"] = tag;
  jsonDocument["value"] = value;
  jsonDocument["unit"] = unit;
  serializeJson(jsonDocument, buffer);
}
 
void add_json_object(char *tag, float value, char *unit) {
  JsonObject obj = jsonDocument.createNestedObject();
  obj["type"] = tag;
  obj["value"] = value;
  obj["unit"] = unit; 
}

void read_sensor_data(void * parameter) {
   for (;;) {
     temperature = dht.readTemperature(true);
     humidity = dht.readHumidity();
     pressure = dht.computeHeatIndex(temperature, humidity, false);
     Serial.println("Read sensor data");
 
     vTaskDelay(60000 / portTICK_PERIOD_MS);
   }
}
 
void getTemperature() {
  Serial.println("Get temperature");
  create_json("temperature", temperature, "°F");
  server.send(200, "application/json", buffer);
}
 
void getHumidity() {
  Serial.println("Get humidity");
  create_json("humidity", humidity, "%");
  server.send(200, "application/json", buffer);
}
 
void getPressure() {
  Serial.println("Get pressure");
  create_json("pressure", pressure, "hPa");
  server.send(200, "application/json", buffer);
}
 
void getData() {
  Serial.println("Get DHT11 Sensor Data");
  jsonDocument.clear();
  add_json_object("temperature", temperature, "°F");
  add_json_object("humidity", humidity, "%");
  add_json_object("pressure", pressure, "hPa");
  serializeJson(jsonDocument, buffer);
  server.send(200, "application/json", buffer);
}

void rgbLEDs() {
  if (server.hasArg("plain") == false) {
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);

  int red_value = jsonDocument["red"];
  int green_value = jsonDocument["green"];
  int blue_value = jsonDocument["blue"];

  ledcWrite(redChannel, red_value);
  ledcWrite(greenChannel,green_value);
  ledcWrite(blueChannel, blue_value);

  server.send(200, "application/json", "{}");
}

void setup_task() {    
  xTaskCreate(     
  read_sensor_data,      
  "Read sensor data",      
  1000,      
  NULL,      
  1,     
  NULL     
  );     
}

void setup() {     
  WiFiServer server(80);
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // it is a good practice to make sure your code sets wifi mode how you want it.

  Serial.begin(115200); 
  
  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;
  //wm.resetSettings();
  bool res;
  res = wm.autoConnect(); // auto generated AP name from chipid
  
  ledcSetup(redChannel, frequency, resolution);
  ledcSetup(greenChannel, frequency, resolution);
  ledcSetup(blueChannel, frequency, resolution);
 
  ledcAttachPin(red_pin, redChannel);
  ledcAttachPin(green_pin, greenChannel);
  ledcAttachPin(blue_pin, blueChannel);


    if (!res) {
    Serial.println("Failed to Connect");
    // ESP.restart();
  }
  else {
    //if you get here you have connected to the WiFi
    Serial.println("Connected Successfully! :)");
    

  }
  setup_task();    
  setup_routing();     

     if (isnan(humidity) || isnan(temperature) || isnan(pressure)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }     
}    
       
void loop() {    
  server.handleClient();    
  
    delay(1000);
    Serial.print(" Active Firmware Version:");
    Serial.println(FirmwareVer);

    if (WiFi.status() != WL_CONNECTED) {
        reconnect();
    }

    if (Serial.available() > 0) {
        incomingByte = Serial.read();
        if (incomingByte == 'U') {
            Serial.println("Firmware Update In Progress..");
            if (FirmwareVersionCheck()) {
                firmwareUpdate();
            }
        }
    } 
}

void reconnect() {

                ESP.restart();

}

void firmwareUpdate(void) {
  if (FirmwareVersionCheck()) {
                
    WiFiClientSecure client;
    client.setCACert(rootCACertificate);
    t_httpUpdate_return ret = httpUpdate.update(client, URL_fw_Bin);

    switch (ret) {
    case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
        break;

    case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

    case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
}
}

int FirmwareVersionCheck(void) {
    String payload;
    int httpCode;
    String FirmwareURL = "";
    FirmwareURL += URL_fw_Version;
    FirmwareURL += "?";
    FirmwareURL += String(rand());
    Serial.println(FirmwareURL);
    WiFiClientSecure * client = new WiFiClientSecure;

    if (client) {
        client -> setCACert(rootCACertificate);
        HTTPClient https;

        if (https.begin( * client, FirmwareURL)) {
            Serial.print("[HTTPS] GET...\n");
            // start connection and send HTTP header
            delay(100);
            httpCode = https.GET();
            delay(100);
            if (httpCode == HTTP_CODE_OK) // if version received
            {
                payload = https.getString(); // save received version
            } else {
                Serial.print("Error Occured During Version Check: ");
                Serial.println(httpCode);
            }
            https.end();
        }
        delete client;
    }

    if (httpCode == HTTP_CODE_OK) // if version received
    {
        payload.trim();
        if (payload.equals(FirmwareVer)) {
            Serial.printf("\nDevice  IS Already on Latest Firmware Version:%s\n", FirmwareVer);
            return 0;
        } else {
            Serial.println(payload);
            Serial.println("New Firmware Detected");
            return 1;
        }
    }
    return 0;
}
