/**
 * ELEC-H423 Project - Key Generation with Morse Code and PBKDF2
 
 */

#include <Arduino.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/md.h>

const int BUTTON_PIN = 27;     
const int LED_PIN = 33;        

// morse timing
const unsigned long DOT_MAX_MS  = 400;  
const unsigned long DASH_MIN_MS = 401;  
const unsigned long CHAR_SPACE_MIN_MS = 1500; 

const unsigned long DEBOUNCE_MS = 30;

// Target password
const String targetPassword = "ACYL";
const int MAX_PASSWORD_LEN = 4;

int debouncedState = HIGH;    
int lastDebouncedState = HIGH;
unsigned long lastDebounceTime = 0;

unsigned long pressStartTime = 0;
unsigned long lastElementTime = 0;

String currentMorse = "";
String userPassword = "";

//PBKDF2 IMPLEMENTATION using HMAC-SHA256

bool deriveKeyPBKDF2(const String& password, const uint8_t* salt,
             size_t saltLen, uint32_t iterations,
             size_t dkLen, uint8_t* derivedKey) 
{
    Serial.println("\n--- PBKDF2 Key Derivation Started (REAL) ---");
    Serial.print("Password: ");
    Serial.println(password);


    const unsigned char* passwordCstr = (const unsigned char*)password.c_str();
    size_t passwordLen = password.length();

    int result = mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA256, 
        passwordCstr, passwordLen,
        salt, saltLen,
        iterations,
        dkLen,
        derivedKey
    );

    if (result == 0) {
        Serial.println("PBKDF2 Key Derivation Complete (Success).");
        
        // --- MODIFIED KEY PRINTING: Continuous Hex String ---
        Serial.print("Derived Key: ");
        for (int i = 0; i < dkLen; i++) {
            // Ensures two digits are printed (e.g., "0A" instead of "A")
            if (derivedKey[i] < 0x10) { 
                Serial.print("0"); 
            }
            // Print the byte as a hexadecimal number
            Serial.print(derivedKey[i], HEX); 
        }
        Serial.println(); // Newline after the key
        // ----------------------------------------------------
        
        return true;
    } else {
        Serial.print("Error during PBKDF2 derivation! Mbed-TLS error code: 0x");
        Serial.println(result, HEX);
        return false;
    }
}

//Morse table
struct MorseMap {
    char ch;
    const char* code;
};

MorseMap morseTable[] = {
    {'A', ".-"},
    {'C', "-.-."},
    {'Y', "-.--"},
    {'L', ".-.."}
};

int MORSE_TABLE_SIZE = sizeof(morseTable) / sizeof(morseTable[0]);

int debounceButton() {
    static int lastRawState = HIGH;
    int rawState = digitalRead(BUTTON_PIN);

    if (rawState != lastRawState) {
        lastDebounceTime = millis();    // state changed → restart timer
    }

    // state stable for at least DEBOUNCE_MS
    if (millis() - lastDebounceTime > DEBOUNCE_MS) {
        debouncedState = rawState;
    }

    lastRawState = rawState;
    return debouncedState;
}

// PROCESS DOT/DASH
void processPressDuration(unsigned long duration) {

    if (duration >= DASH_MIN_MS) { 
        currentMorse += "-";
        Serial.println("Dash");
    }
    else if (duration >= 40 && duration <= DOT_MAX_MS) {
        currentMorse += ".";
        Serial.println("Dot");
    }
    else {
        Serial.print("Ignored press: ");
        Serial.println(duration);
    }

    lastElementTime = millis();
}

// TRANSLATE MORSE TO CHAR
void translateCurrentMorse() {
    if (currentMorse.length() == 0) return;

    Serial.print("Translating: ");
    Serial.println(currentMorse);

    bool found = false;
    for (int i = 0; i < MORSE_TABLE_SIZE; i++) {
        if (currentMorse == morseTable[i].code) {
            found = true;

            if (userPassword.length() < MAX_PASSWORD_LEN) {
                userPassword += morseTable[i].ch;
                Serial.print("→ Added: ");
                Serial.println(morseTable[i].ch);

                digitalWrite(LED_PIN, HIGH);
                delay(80);
                digitalWrite(LED_PIN, LOW);
            }
            break;
        }
    }

    if (!found) {
        Serial.println("Invalid Morse sequence");
    }

    currentMorse = "";
}

void setup() {
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);

    Serial.println("==== Morse Password + PBKDF2 System Ready ====");
    Serial.println("Enter Password in morse:");
}

void loop() {

    int state = debounceButton();

    static int lastState = HIGH;
    bool pressed = (lastState == HIGH && state == LOW);
    bool released = (lastState == LOW && state == HIGH);

    if (pressed) {
        pressStartTime = millis();
    }

    if (released) {
        unsigned long duration = millis() - pressStartTime;
        processPressDuration(duration);
    }

    lastState = state;

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

                uint8_t salt[8] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11}; 
                
                uint8_t keyOut[16]; 
                const size_t DK_LEN = 16;
                const uint32_t ITERATIONS = 10000; 

                deriveKeyPBKDF2(userPassword, salt, sizeof(salt), ITERATIONS, DK_LEN, keyOut);

                userPassword = ""; // Reset for next input
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