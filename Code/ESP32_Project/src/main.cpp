#include <Arduino.h>
#include "DHTesp.h"
#include <Ticker.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <PubSubClient.h>
// Local network Dashboard
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// MQTT topic set-up
#define humidity_topic "sensor/DHT11/humidity"
#define temperature_topic "sensor/DHT11/temperature"
#define heatIndex_topic "sensor/DHT11/heatIndex"
#define dewPoint_topic "sensor/DHT11/dewPoint"
#define Comfort_topic "sensor/DHT11/Comfort"

// Webserver

AsyncWebServer server(80);

// WIFI Set-up
// HOME WIFI
//#define WIFI_SSID "CPE" // WIFI
//#define wifi_pass "cpe7728110cpe7728110"

// HOME 2 WIFI
//#define WIFI_SSID "VOO-Y9SVPR4" // WIFI
//#define wifi_pass "g4Er7ZamEY9Y4b4HGa"

// HOTSPOT
#define WIFI_SSID "AndroidAP" // MOBILE HOTSPOT
#define wifi_pass "coucou67"

DHTesp dht;

// MQTT Broker
// HOME WIFI
//const char* mqtt_server = "192.168.0.133";

// HOTSPOT
const char* mqtt_server = "192.168.43.86";

WiFiClient espClient;
PubSubClient client(espClient);

// Function Def
void getTemperature();
void publishData();

// IO
const int buttonPin = 27;        // the number of the pushbutton pin
const int ledPinWhite = 33;      // the number of the LED pin white
const int ledPinRed = 32;        // the number of the LED pin red

IPAddress gatewayIP;

// Global Variables:
float g_temp, g_hum, g_heatIndex, g_dewPoint, g_cr, g_threshold;
int g_interval;
int buttonState = 0;  // variable for reading the pushbutton status
TaskHandle_t tempTaskHandle = NULL; // Thread pointer
Ticker tempTicker(getTemperature,10000,0,MILLIS); // Interrupt
Ticker PublishTicker(publishData,20000,0,MILLIS); // Interrupt publishment on Mosquitto
ComfortState cf;  // as it is written
bool tasksEnabled = false; // Flag to disbale sensor if needed
int dhtPin = 26;

bool initTemp() {
  byte resultValue = 0;
  // Initialize temperature sensor
	dht.setup(dhtPin, DHTesp::DHT11);
	Serial.println("DHT initiated");

  // Task Set-up - Thread creation
	// xTaskCreatePinnedToCore(
	// 		tempTask,                       /* Function to implement the task */
	// 		"tempTask ",                    /* Name of the task */
	// 		4000,                           /* Stack size in words */
	// 		NULL,                           /* Task input parameter */
	// 		5,                              /* Priority of the task */
	// 		&tempTaskHandle,                /* Task handle. */
	// 		1);                             /* Core where the task should run */

  // if (tempTaskHandle == NULL) {
  //   Serial.println("Failed to point to created thread");
  //   return false;
  // } else {
    // Start update of environment data every 20 seconds
    tempTicker.start();
    PublishTicker.start();
    //tempTicker2 = new Ticker(triggerGetTemp,20000);
    //tempTicker2->start();
  //}
  return true;
}

// Get data from sensors
void getTemperature() {
  TempAndHumidity newValues = dht.getTempAndHumidity();
	// Check if any reads failed and exit early (to try again).
	if (dht.getStatus() != 0) {
		Serial.println("DHT11 error status: " + String(dht.getStatusString()));
		return;
	}
  g_temp = newValues.temperature;
  g_hum = newValues.humidity;
	g_heatIndex = dht.computeHeatIndex(newValues.temperature, newValues.humidity);
  g_dewPoint = dht.computeDewPoint(newValues.temperature, newValues.humidity);
  g_cr = dht.getComfortRatio(cf, newValues.temperature, newValues.humidity);

  String comfortStatus;
  switch(cf) {
    case Comfort_OK:
      comfortStatus = "Comfort_OK";
      break;
    case Comfort_TooHot:
      comfortStatus = "Comfort_TooHot";
      break;
    case Comfort_TooCold:
      comfortStatus = "Comfort_TooCold";
      break;
    case Comfort_TooDry:
      comfortStatus = "Comfort_TooDry";
      break;
    case Comfort_TooHumid:
      comfortStatus = "Comfort_TooHumid";
      break;
    case Comfort_HotAndHumid:
      comfortStatus = "Comfort_HotAndHumid";
      break;
    case Comfort_HotAndDry:
      comfortStatus = "Comfort_HotAndDry";
      break;
    case Comfort_ColdAndHumid:
      comfortStatus = "Comfort_ColdAndHumid";
      break;
    case Comfort_ColdAndDry:
      comfortStatus = "Comfort_ColdAndDry";
      break;
    default:
      comfortStatus = "Unknown:";
      break;
  };
  
  Serial.println(" Temp:" + String(g_temp) + " Hum:" + String(g_hum) + " Index:" + String(g_heatIndex) + " Dew:" + String(g_dewPoint) + " " + comfortStatus);
}

void publishData(){
  if (!isnan(g_temp) || !isnan(g_hum) || !isnan(g_heatIndex) || !isnan(g_dewPoint) ||!isnan(g_cr)) {
    // char tempString[8];
    // dtostrf(t, 1, 2, tempString);

    Serial.println("***** PUBLISHMENT OF DATA TO MQTT SERVER****");
    client.publish(humidity_topic, String(g_hum).c_str(),true);
    Serial.print("Humidity in % : ");
    Serial.println(String(g_hum).c_str());

    client.publish(temperature_topic, String(g_temp).c_str(),true);
    Serial.print("Temp in celsius : ");
    Serial.println(String(g_temp).c_str());

    client.publish(heatIndex_topic, String(g_heatIndex).c_str(),true);
    Serial.print("Heat index : ");
    Serial.println(String(g_heatIndex).c_str());

    client.publish(dewPoint_topic, String(g_dewPoint).c_str(),true);
    Serial.print("Dew point : ");
    Serial.println(String(g_dewPoint).c_str());

    client.publish(Comfort_topic, String(g_cr).c_str(),true);
    Serial.print("Comfort : ");
    Serial.println(String(g_cr).c_str());
    // turn LED on:
    digitalWrite(ledPinRed, HIGH);
    delay(1000);
    digitalWrite(ledPinRed,LOW);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  Serial.print("Message: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup_wifi() {

    Serial.println();
    Serial.println("Starting WiFi connection...");

    WiFi.mode(WIFI_STA);          // Station mode
    WiFi.disconnect(true);        // Disconnect from previous networks
    delay(100);
    WiFi.begin(WIFI_SSID,wifi_pass);

    Serial.print("Connecting to WiFi");
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        Serial.print(WiFi.status());
    }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    String Ip_esp32 = String(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi. Check SSID, credentials, or WPA2 Enterprise support.");
  }

  digitalWrite(ledPinWhite,HIGH);
  delay(500);
  digitalWrite(ledPinWhite, LOW);
  delay(500);
  digitalWrite(ledPinWhite,HIGH);
  delay(500);
  digitalWrite(ledPinWhite, LOW);

}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32_TempPublisher")) {
      Serial.println("Connected!");
      client.subscribe(humidity_topic);
      client.subscribe(temperature_topic);
      client.subscribe(heatIndex_topic);
      client.subscribe(dewPoint_topic);
      client.subscribe(Comfort_topic);
      digitalWrite(ledPinWhite,HIGH);
      delay(100);
      digitalWrite(ledPinWhite, LOW);
      delay(100);
      digitalWrite(ledPinRed,HIGH);
      delay(100);
      digitalWrite(ledPinRed, LOW);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void initWebServer(){

  if (!LittleFS.begin(true)){
    Serial.println("Could not build the file system containing WebServer info");
    return;
  }

  Serial.println("File System build ! - Let's proceed to WebServer Design");

  server.serveStatic("/",LittleFS,"/").setDefaultFile("index.html");

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"temperature\":" + String(g_temp) + "}";
    request -> send(200,"application/json",json);
  });

  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"humidity\":" + String(g_hum) + "}";
    request -> send(200,"application/json",json);
  });

  server.on("/confort", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"confort\":" + String(g_cr) + "}";
    request -> send(200,"application/json",json);
  });

  server.on("/dewpoint", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"dewpoint\":" + String(g_dewPoint) + "}";
    request -> send(200,"application/json",json);
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"threshold\": 30, \"interval\" : 10}";
    request -> send(200,"application/json",json);
  });

  server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t * data, size_t len, size_t index, size_t total){
    
    String message;
    if (index == 0) {
      message = "";
    }

    for (size_t i = 0; i < len; i++){
      message +=(char)data[i];
    }


    if (index + len == total){
      DynamicJsonDocument doc(256);
      deserializeJson(doc, message);
      float threshold = doc["threshold"];
      int interval = doc["interval"];
      
      // store these in global variables
      g_threshold = threshold;
      g_interval = interval;
    }

      Serial.println("Update requested !! - " + message);
      request -> send(200,"text/plain","Settings got updated");
  });

  server.begin();
  Serial.println("Web server started");

  //Serial.println("Listing LittleFS files:");
  //File root = LittleFS.open("/");
  //File file = root.openNextFile();
  //while (file) {
  //    Serial.println("FILE: " + String(file.name()));
  //    file = root.openNextFile();
  //}

}

void setup() {

  Serial.begin(115200);
  // initialize the LED pin as an output:
  pinMode(ledPinWhite, OUTPUT);
  pinMode(ledPinRed, OUTPUT);
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);

  setup_wifi();

  initWebServer();

  client.setServer(mqtt_server, 1883); 
  client.setCallback(callback);
  initTemp();
  // Signal end of setup() to tasks
  tasksEnabled = true;
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  tempTicker.update();
  PublishTicker.update();
  // read the state of the pushbutton value:
  buttonState = digitalRead(buttonPin);

  // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
  if (buttonState == HIGH) {
    // turn LED on:
    digitalWrite(ledPinWhite, HIGH);
  } else {
    // turn LED off:
    digitalWrite(ledPinWhite, LOW);
  }
}
