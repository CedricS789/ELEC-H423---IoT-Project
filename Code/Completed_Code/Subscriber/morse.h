/**
 * ELEC-H423 Project - Key Generation with Morse Code and PBKDF2
 
 */

#pragma once                // Avoid multiple usage of the files
#include <Arduino.h>
#include "mbedtls/pkcs5.h"
#include <mbedtls/md.h>
#include "aes.h"

const int BUTTON_PIN = 27;     
const int LED_PIN = 33;
extern uint8_t key_plain[16];     // passing the key to h file

// morse timing
const unsigned long DOT_MAX_MS  = 500;  
const unsigned long DASH_MIN_MS = 501;  
const unsigned long CHAR_SPACE_MIN_MS = 2000; 

const unsigned long DEBOUNCE_MS = 50;

// Target password
const String targetPassword = "A";
const int MAX_PASSWORD_LEN = 1;


enum PressState { IDLE, PRESSED };
PressState pressState = IDLE;

unsigned long pressStart = 0;
unsigned long lastStableRead = 0;

int debouncedState = HIGH;
unsigned long lastDebounceTime = 0;

//unsigned long pressStartTime = 0;
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

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);

    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    // Initialize the context with the hash info
    int ret = mbedtls_md_setup(&ctx, md, 1); // 1 = HMAC
    if (ret != 0) {
        Serial.print("mbedtls_md_setup failed: ");
        Serial.println(ret, HEX);
        return false;
    }

    int result = mbedtls_pkcs5_pbkdf2_hmac(
        &ctx, 
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

        Serial.println();

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

    int raw = digitalRead(BUTTON_PIN);
    unsigned long now = millis();

    static int lastRaw = LOW;
    static int stableState = LOW;

    // --- 1. Detect stable state (debounce) ---
    if (raw != lastRaw) {
        lastStableRead = now;      // state changed → restart timer
    }

    if (now - lastStableRead >= DEBOUNCE_MS) {
        stableState = raw;         // stable for enough time → valid
    }

    lastRaw = raw;

    return stableState;
}

// PROCESS DOT/DASH
void handlePress(unsigned long duration) {

    Serial.print("Duration: ");
    Serial.println(duration);

    if (duration < 120) {   // ignore super-short spikes
        Serial.println("Ignored bounce");
        return;
    }

    if (duration <= DOT_MAX_MS) {
        currentMorse += ".";
        Serial.println("Dot");
    }
    else if (duration >= DASH_MIN_MS) {
        currentMorse += "-";
        Serial.println("Dash");
    }
    else {
        Serial.println("Invalid duration");
        return;
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