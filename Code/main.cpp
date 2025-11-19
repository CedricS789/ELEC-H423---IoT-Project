#include <Arduino.h>
#include "DHTesp.h"
#include <Ticker.h>
#include <WiFi.h>
#include <PubSubClient.h>

// MQTT topic set-up
#define humidity_topic "sensor/DHT11/humidity"
#define temperature_topic "sensor/DHT11/temperature"
#define heatIndex_topic "sensor/DHT11/heatIndex"
#define dewPoint_topic "sensor/DHT11/dewPoint"
#define Comfort_topic "sensor/DHT11/Comfort"

// WIFI Set-up
#define wifi_ssid "CPE"
#define wifi_pass "cpe7728110cpe7728110"

DHTesp dht;

// MQTT Broker
const char* mqtt_server = "192.168.1.101";

WiFiClient espClient;
PubSubClient client(espClient);

// Function Def
void getTemperature();
void publishData();

// IO
const int buttonPin = 27;        // the number of the pushbutton pin
const int ledPinWhite = 33;      // the number of the LED pin white
const int ledPinRed = 32;        // the number of the LED pin red

// variables will change:
float g_temp, g_hum, g_heatIndex, g_dewPoint, g_cr;
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

    client.publish(heatIndex_topic, String(g_dewPoint).c_str(),true);
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
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(WiFi.status());
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");

  digitalWrite(ledPinWhite,HIGH);
  delay(500);
  digitalWrite(ledPinWhite, LOW);
  delay(500);
  digitalWrite(ledPinWhite,HIGH);
  delay(500);
  digitalWrite(ledPinWhite, LOW);
  Serial.println(WiFi.localIP());
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

void setup() {

  Serial.begin(115200);
  // initialize the LED pin as an output:
  pinMode(ledPinWhite, OUTPUT);
  pinMode(ledPinRed, OUTPUT);
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
  setup_wifi();
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
