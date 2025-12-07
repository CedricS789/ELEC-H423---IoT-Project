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

#define DHT11_all "sensor/DHT11/all"
#define DHT11_HMAC "sensor/DHT11/all_Hmac"


// WIFI Set-up
// HOME WIFI

// HOTSPOT
#define WIFI_SSID "AndroidAP" // MOBILE HOTSPOT
#define wifi_pass "coucou67"

DHTesp dht;

// MQTT Broker
// HOME WIFI

// HOTSPOT
const char* mqtt_server = "192.168.43.86";

WiFiClient espClient;
PubSubClient client(espClient);

// Function Def
void getTemperature();
void publishData();
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
uint32_t tx_glob_counter;
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
Ticker PublishTicker(publishData,20000,0,MILLIS); // Interrupt publishment on Mosquitto
// Port Knocking
const int port[] = {1883, 1900, 1925, 1960};
int current_index = 0;
int current_port = port[current_index];
bool portswitchbool = false;
unsigned long timerforswitch = 0;

//************Functions******************
bool initTemp() {
  byte resultValue = 0;
  // Initialize temperature sensor
	dht.setup(dhtPin, DHTesp::DHT11);
	Serial.println("DHT initiated");
  tempTicker.start();
  PublishTicker.start();
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

    Serial.println("***** PUBLISHMENT OF ENCRYPTED DATA TO MQTT SERVER****");

    DynamicJsonDocument doc(256);
    doc["temp"] = g_temp;
    doc["hum"] = g_hum;
    doc["heat"] = g_heatIndex;
    doc["dew"] = g_dewPoint;
    doc["comfort"] = g_cr;

    String payload;
    serializeJson(doc, payload);

    Serial.print("Sending json doc");
    Serial.println(payload);

    tx_glob_counter++;
    write_counter_flash_tx(tx_glob_counter);

    String encrypted = aes_encrypt(payload);
    String hmac_message = Hmac_encrypt(encrypted);

    client.publish(DHT11_all, encrypted.c_str(), false);
    client.publish(DHT11_HMAC, hmac_message.c_str(), false);

    Serial.print("HMAC");
    Serial.print(" => ");
    Serial.println(hmac_message);

    Serial.print("Encrypted Data");
    Serial.print(" => ");
    Serial.println(encrypted);

    portswitchbool = true;
    timerforswitch = millis()+1000;
    
    // turn LED on:
    digitalWrite(ledPinRed, HIGH);
    delay(1000);
    digitalWrite(ledPinRed,LOW);
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


//************Set up******************
void setup() {

  Serial.begin(115200);
  // Write Key in flash
  // write_key_flash();
  // Load Key in RAM
  load_key_flash();
  // Reset counters properly
  tx_glob_counter = 0;

  write_counter_flash_tx(tx_glob_counter);
  
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


  client.setServer(mqtt_server, 1883); 

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


    if (portswitchbool && millis() > timerforswitch){
      portswitchbool = false;
      //PublishVisuTicker.stop();
      current_index = (current_index + 1) % (sizeof(port)/sizeof(port[0]));
      current_port = port[current_index];

      Serial.print("Switching Suscribed port : ");
      Serial.println(current_port);

      client.disconnect();
      client.setServer(mqtt_server, current_port);
      reconnect();
      client.subscribe(DHT11_all);
      client.subscribe(DHT11_HMAC);
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
    } 
    else {
      // turn LED off:
      digitalWrite(ledPinWhite, LOW);
    }
  }

}

