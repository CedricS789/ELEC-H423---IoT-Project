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
// ENCRYPTION 
#include "aes.h"
#include "Preferences.h"    // Generate the key and 
// PBKDF2
#include "morse.h"

// MQTT topic set-up
#define humidity_topic "visual/humidity"
#define temperature_topic "visual/temperature"
#define heatIndex_topic "visual/heatIndex"
#define dewPoint_topic "visual/dewPoint"
#define Comfort_topic "visual/Comfort"

#define DHT11_all "sensor/DHT11/all"
#define DHT11_HMAC "sensor/DHT11/all_Hmac"

// Webserver

AsyncWebServer server(80);

// WIFI Set-up
// HOME WIFI

// HOTSPOT
#define WIFI_SSID "AndroidAP" // MOBILE HOTSPOT
#define wifi_pass "coucou67"

DHTesp dht;

// MQTT Broker
// HOME WIFI

// HOTSPOT
const char* mqtt_server_encrypt = "192.168.43.86";
const char* mqtt_server_visu = "192.168.43.86";

WiFiClient espClient;
PubSubClient client(espClient);

WiFiClient espClientPlain;
PubSubClient clientPlain(espClientPlain);

// Function Def
void getTemperature();
void publishData_rx();
void reconnect();

// IO
const int buttonPin = 27;        // the number of the pushbutton pin
const int ledPinWhite = 33;      // the number of the LED pin white
const int ledPinRed = 32;        // the number of the LED pin red

IPAddress gatewayIP;

// ************Global Variables******************
// Temp init
float g_temp, g_hum, g_heatIndex, g_dewPoint, g_cr, g_threshold;
ComfortState cf;  // as it is written
bool tasksEnabled = false; // Flag to disbale sensor if needed
int dhtPin = 26;
// AES and counter init
uint8_t key_plain [16];     // RAM
uint8_t key_encrypt [32];   // RAM
uint32_t rx_glob_counter;
// HMAC verif init
String lastEncryptedPayload = "";
String lastHMAC = "";
//Morse init for loop()
bool morsecode = false;
// Push button init
int g_interval;
int buttonState = 0;  // variable for reading the pushbutton status
// Thread creation init - not needed anymore
TaskHandle_t tempTaskHandle = NULL; // Thread pointer
// Ticker for void loop()
Ticker tempTicker(getTemperature,10000,0,MILLIS); // Interrupt
Ticker PublishVisuTicker(publishData_rx,20000,0,MILLIS); // Interrupt publishment for plain data visualisation
// Port Knocking
const int port[] = {1883, 1900, 1925, 1960};
int current_index = 0;
int current_port = port[current_index];
bool portswitchbool = false;
const int port_visu = 1950;
unsigned long timerforswitch = 0;

//************Functions******************
bool initTemp() {
  byte resultValue = 0;
  // Initialize temperature sensor
	dht.setup(dhtPin, DHTesp::DHT11);
	Serial.println("DHT initiated");
  tempTicker.start();
  PublishVisuTicker.start();
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


void publishData_rx(){
  if (!isnan(g_temp) || !isnan(g_hum) || !isnan(g_heatIndex) || !isnan(g_dewPoint) ||!isnan(g_cr)) {
    // char tempString[8];
    // dtostrf(t, 1, 2, tempString);

    Serial.println("***** PUBLISHMENT OF PLAIN DATA TO MQTT SERVER****");

    clientPlain.publish(temperature_topic, String(g_temp,2).c_str(), true);
    clientPlain.publish(humidity_topic, String(g_hum,2).c_str(), true);
    clientPlain.publish(dewPoint_topic, String(g_dewPoint,2).c_str(), true);
    clientPlain.publish(heatIndex_topic, String(g_heatIndex,2).c_str(), true);
    clientPlain.publish(Comfort_topic, String(g_cr,2).c_str(), true);

    Serial.println("Sending data to Port 1950 for data Visualisation");
    
    // turn LED on:
    digitalWrite(ledPinRed, HIGH);
    delay(1000);
    digitalWrite(ledPinRed,LOW);

    portswitchbool = true;
    timerforswitch = millis()+1000;

  }
}

bool ignoreFirstPayload = true;
bool ignoreFirstHMAC = true;

void callback(char* topic, byte* payload, unsigned int length) {

  if (String(topic) == DHT11_all && ignoreFirstPayload) {
        Serial.println("Ignoring first retained payload");
        ignoreFirstPayload = false;
        return;
  }

  if (String(topic) == DHT11_HMAC && ignoreFirstHMAC) {
        Serial.println("Ignoring first retained HMAC");
        ignoreFirstHMAC = false;
        return;
  }
  
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);

  Serial.print("Message: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  if (String(topic) == DHT11_all) {
    lastEncryptedPayload = msg;

  } else if (String(topic) == DHT11_HMAC) {
    lastHMAC = msg;

    // Now verify HMAC
    if (verify_HMAC(lastEncryptedPayload, lastHMAC)) {
      uint32_t decrypted_counter = 0;
      String decrypted = aes_decrypt(lastEncryptedPayload, &decrypted_counter);

      
      if (decrypted.length() == 0) {
          Serial.println("Received packet is invalid or replayed. Discarding.");
          return;  // Stop further processing
      }

      // Parse JSON
      DynamicJsonDocument doc(256);
      deserializeJson(doc, decrypted);

      Serial.println("Decrypted json doc:");
      Serial.println(decrypted);

      float temp = doc["temp"];
      float hum = doc["hum"];
      float heat = doc["heat"];
      float dew = doc["dew"];
      int comfort = doc["comfort"];

      Serial.println("Decrypted values:");
      Serial.println("Temp: " + String(temp));
      Serial.println("Hum: " + String(hum));
      Serial.println("Heat: " + String(heat));
      Serial.println("Dew: " + String(dew));
      Serial.println("Comfort: " + String(comfort));
      Serial.println("Counter: " + String(decrypted_counter));

    } else {
      Serial.println("HMAC verification failed! Discarding message.");
    }
  }
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
  ignoreFirstPayload = true;
  ignoreFirstHMAC = true;
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32_TempPublisher")) {
      Serial.println("Connected!");

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

}


//************Set up******************
void setup() {

  Serial.begin(115200);
  // Write Key in flash
  write_key_flash();
  // Load Key in RAM
  load_key_flash();
  // Reset counters properly
  rx_glob_counter = 0;

  write_counter_flash_rx(rx_glob_counter);
  
  // Get the counter
  get_counter_flash_tx();
  // Get the counter
  get_counter_flash_rx();
  // initialize the LED pin as an output:
  pinMode(ledPinWhite, OUTPUT);
  pinMode(ledPinRed, OUTPUT);
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);

  setup_wifi();

  initWebServer();

  client.setServer(mqtt_server_encrypt, 1883); 
  client.setCallback(callback);
  clientPlain.setServer(mqtt_server_visu, 1950); 

  initTemp();

  Serial.println("==== ENTER THE CODE TO DECODE THE KEY ====");
  Serial.println("Enter Password in morse:");
}


//************Loop******************
void loop() {

  if (!morsecode){
    int state = debounceButton();
    unsigned long now = millis();

    if (pressState == IDLE && state == HIGH) {
      // BUTTON PRESSED
        pressStart = now;
        pressState = PRESSED;
    }

    if (pressState == PRESSED && state == LOW) {
      // BUTTON RELEASED
      unsigned long duration = now - pressStart;
      handlePress(duration);
      pressState = IDLE;
    }

    // CHARACTER SPACING 
    if (currentMorse.length() > 0 &&
      millis() - lastElementTime > CHAR_SPACE_MIN_MS) {

      translateCurrentMorse();

      Serial.print("Password so far: ");
      Serial.println(userPassword);

          
      if (userPassword.length() == MAX_PASSWORD_LEN) {
              
        if (userPassword == targetPassword) {
          Serial.println("\n✔ PASSWORD ACCEPTED: ACYL");
          digitalWrite(LED_PIN, HIGH);
          delay(500);
          digitalWrite(LED_PIN, LOW);
          morsecode = true;

          uint8_t salt[8] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11}; 
                  
          uint8_t keyOut[16]; 
          const size_t DK_LEN = 16;
          const uint32_t ITERATIONS = 10000; 

          deriveKeyPBKDF2(userPassword, salt, sizeof(salt), ITERATIONS, DK_LEN, keyOut);

          userPassword = ""; // Reset for next input

          String key_string = text_encoder(key_encrypt,32);
          aes_decrypt_key(key_string, keyOut, key_plain);
          Serial.print("Print plain key : "); 
          for (int i = 0; i < 16; i++) {
              // Ensures two digits are printed (e.g., "0A" instead of "A")
              if (key_plain[i] < 0x10) { 
                  Serial.print("0"); 
              }
              // Print the byte as a hexadecimal number
              Serial.print(key_plain[i], HEX); 
          }
          Serial.println(); // Newline after the key
        
        } 
        else {
            Serial.println("PASSWORD REJECTED. Encoded: " + userPassword);
            Serial.println("Please enter the password again:");
                  
            for(int i = 0; i < 3; i++){
              digitalWrite(LED_PIN, HIGH);
              delay(150);
              digitalWrite(LED_PIN, LOW);
              delay(150);
            }

            userPassword = "";
        }
      }
    }
  }
  else{

    while (WiFi.status() != WL_CONNECTED){
      Serial.println("Wifi is disconnected !, reconnecting ....");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID,wifi_pass);
      delay(5000);
    }
    if (!client.connected()) {
      Serial.print("Wifi Status : ");
      Serial.println(WiFi.status());
      client.disconnect();
      reconnect();
      Serial.println("***** SUBSCRIPTION OF ENCRYPTED DATA FROM MQTT SERVER****");
      client.subscribe(DHT11_all);
      client.subscribe(DHT11_HMAC);
    }

    if (!clientPlain.connected()) {
      while(!clientPlain.connected()){
        clientPlain.connect("ESP_VisualClient");
        delay(500);
      }
    }

    if (portswitchbool && millis() > timerforswitch){
      portswitchbool = false;
      //PublishVisuTicker.stop();
      current_index = (current_index + 1) % (sizeof(port)/sizeof(port[0]));
      current_port = port[current_index];

      Serial.print("Switching Suscribed port : ");
      Serial.println(current_port);

      client.disconnect();
      client.setServer(mqtt_server_encrypt, current_port);
      reconnect();
      client.subscribe(DHT11_all);
      client.subscribe(DHT11_HMAC);
    }

    client.loop();
    clientPlain.loop();

    tempTicker.update();

    PublishVisuTicker.update();

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
}
