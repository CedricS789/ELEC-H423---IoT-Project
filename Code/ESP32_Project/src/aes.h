#pragma once                // Avoid multiple usage of the files
#include <Arduino.h>
#include "Preferences.h"    // Use the flash memory
#include "mbedtls/aes.h"    // AES Accelerator
#include "mbedtls/base64.h" // base64 decode
#include "mbedtls/md.h"     // HMAC
#include "esp_system.h"     // Random byte generation

extern uint8_t AES_KEY[16];     // passing the key to h file
extern uint32_t tx_glob_counter;     // passing the counter to h file
extern uint32_t rx_glob_counter;

Preferences prefs;

void write_key_flash(){

    static const uint8_t key [16] = {0x54, 0x68, 0x61, 0x74, 0x73, 0x20, 0x6d, 0x79, 0x20, 0x4b, 0x75, 0x6e, 0x67, 0x20, 0x46, 0x75};
    prefs.begin("Encryption",false);
    prefs.putBytes("Encrypted_AES",key,sizeof(key));
    prefs.end();
}

void load_key_flash(){
    prefs.begin("Encryption",false);
    prefs.getBytes("Encrypted_AES",AES_KEY,sizeof(AES_KEY));
    prefs.end();
}

void write_counter_flash_rx(uint32_t counter){
    prefs.begin("DOS_Counter_rx",false);
    prefs.putBytes("Packet_counter",&counter,sizeof(counter));
    prefs.end();
}

void get_counter_flash_rx(){
    prefs.begin("DOS_Counter_rx",false);
    prefs.getBytes("Packet_counter",&rx_glob_counter, sizeof(rx_glob_counter));
    prefs.end();
}

void write_counter_flash_tx(uint32_t counter){
    prefs.begin("DOS_Counter_tx",false);
    prefs.putBytes("Packet_counter",&counter,sizeof(counter));
    prefs.end();
}

void get_counter_flash_tx(){
    prefs.begin("DOS_Counter_tx",false);
    prefs.getBytes("Packet_counter",&tx_glob_counter, sizeof(tx_glob_counter));
    prefs.end();
}

void reset_rx_counter() {
    rx_glob_counter = 0; // reset RAM variable
    prefs.begin("DOS_Counter_tx", false);
    prefs.putBytes("Packet_counter", &rx_glob_counter, sizeof(rx_glob_counter));
    prefs.end();
}

void reset_tx_counter() {
    tx_glob_counter = 0; // reset RAM variable
    prefs.begin("DOS_Counter_tx", false);
    prefs.putBytes("Packet_counter", &tx_glob_counter, sizeof(tx_glob_counter));
    prefs.end();
}

String text_encoder(const uint8_t* data, size_t data_len) {

    size_t output_len = 0;
    // Allocate enough space for Base64 output (4/3 expansion + 1 for null char)
    size_t base64_len = 4 * ((data_len + 2) / 3);               // Base64 is 33% bigger in memory than byte data - "+2" to ensure ceiling of data/3

    unsigned char* base64 = (unsigned char*) malloc(base64_len + 1);
    if (!base64) return "";

    int ret = mbedtls_base64_encode(                                // Convert binary to C string
        base64,         // destination buffer
        base64_len + 1, // buffer size
        &output_len,    // effective output len
        data,           // source bytes
        data_len        // source size
    );

    if (ret != 0) {
        free(base64);
        return "";
    }

    base64[output_len] = '\0';   // null terminate

    String out = String((char*)base64);         // Make a Arduino String
    free(base64);
    return out;
}

String aes_encrypt(const String& plaintext){

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    String aes_message = "";

    uint32_t counter_temp = tx_glob_counter;

    size_t input_len = plaintext.length();
    size_t padded_len = (input_len + 15) & ~15;                         // AND logic with 11110000 -> Find the multiple of chunks of 16 bytes

    uint8_t* input_buf = (uint8_t*)calloc(1, padded_len);               // calloc for filling memory with "0"
    memcpy(input_buf, plaintext.c_str(), input_len);

    uint8_t* output_buf = (uint8_t*)calloc(1, padded_len);

    uint8_t AES_IV[16];
    esp_fill_random(AES_IV,16);
    String AES_IV_check = text_encoder(AES_IV,16);
    
    Serial.print("Check AES_IV key : ");
    Serial.println(AES_IV_check);

    String AES_check = text_encoder(AES_KEY,16);
    Serial.print("Check AES key : ");
    Serial.println(AES_check);

    uint8_t aes_IV_copy [16];
    memcpy(aes_IV_copy,AES_IV,16);

    mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_len, AES_IV, input_buf, output_buf);

    size_t final_payload_size = sizeof(aes_IV_copy) + sizeof(counter_temp) + padded_len;
    uint8_t * final_message = (uint8_t *) calloc(1,final_payload_size);

    memcpy(final_message, aes_IV_copy, sizeof(aes_IV_copy) );
    memcpy(final_message + sizeof(aes_IV_copy), &counter_temp, sizeof(counter_temp));
    memcpy(final_message + sizeof(aes_IV_copy) + sizeof(counter_temp), output_buf, padded_len);

    aes_message = text_encoder(final_message,final_payload_size);

    free (input_buf);
    free (output_buf);
    free (final_message);
    mbedtls_aes_free(&aes);

    return aes_message;
}

bool base64_decode_to_buffer(const String& input, uint8_t** output, size_t* output_len)
{
    // just in case
    if (output == nullptr || output_len == nullptr) 
    {
        return false;
    }

    // Get the length of the Base64-encoded input string.
    size_t in_len = input.length();
    // Estimate the maximum size of the decoded data.
    // Base64 encoding expands data by roughly 4/3, so decoded size is ~3/4 of input adding a small extra margin (+4) for safety.
    size_t buf_len = (in_len * 3 + 3) / 4;

    // Allocate a buffer to hold the decoded bytes.
    uint8_t* buf = (uint8_t*) malloc(buf_len);              // AMAU : Not calloc ???
    // just in case
    if (!buf) 
    {
        return false;
    }

    // Use mbedTLS to decode the Base64 string into raw bytes.
    int ret = mbedtls_base64_decode(
        buf,                               // buffer for decoded bytes
        buf_len,                           // Size of the buffer
        output_len,                        // the actual decoded byte count
        (const unsigned char*)input.c_str(), //input data
        in_len                             // Length of the input
    );

    // If the decode function did not return 0, an error occurred.
    if (ret != 0) 
    {
        free(buf);
        return false;
    }

    // pass ownership of the allocated buffer back to the caller.
    *output = buf;
    return true;
}


// Decrypt an AES-128-CBC encrypted message that was encoded as Base64.
// The input is expected to be Base64( IV(16 bytes) || CIPHERTEXT ).
// Returns the decrypted plaintext as an Arduino String, or empty String on error.

// This function match to "main.cpp" so put this function into it. 

String aes_decrypt(const String& ciphertext_b64, uint32_t* out_counter_rx)
{
    // 1) Base64 decode: ciphertext_b64 -> [IV(16 bytes) || CIPHERTEXT]
    uint8_t* decoded = nullptr;
    size_t decoded_len = 0;

    if (!base64_decode_to_buffer(ciphertext_b64, &decoded, &decoded_len)) 
    {
        Serial.println("[AES] Base64 decode failed");
        return String("");
    }

    if (decoded_len < 16) 
    {
        Serial.println("[AES] Decoded data too short");
        free(decoded);
        return String("");
    }

    // 2) Split the decoded data into IV (first 16 bytes) and ciphertext (remaining bytes).
    uint8_t iv[16];
    memcpy(iv, decoded, 16);

    uint32_t counter_rx;
    memcpy(&counter_rx, decoded + 16, sizeof(uint32_t));

    uint32_t rx_last_counter = rx_glob_counter;

    if (counter_rx <= rx_glob_counter) {
        Serial.println("Replay detected");
        free(decoded);
        return String("");
    } else {
        rx_glob_counter = counter_rx;
        write_counter_flash_rx(rx_glob_counter);
    }

    if (out_counter_rx != nullptr){
        *out_counter_rx = counter_rx;
    }

    size_t cipher_len = decoded_len - 16 - sizeof(uint32_t);
    uint8_t* cipher = decoded + 16 + sizeof(uint32_t);

    // 3) Perform AES-128-CBC decryption using mbedTLS.

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    uint8_t* plain = (uint8_t*) calloc(1, cipher_len);
 
    // just in case
    if (!plain) 
    {
        Serial.println("[AES] malloc failed");
        mbedtls_aes_free(&aes);
        free(decoded);
        return String("");
    }

    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, 16);

    // Set up the AES context for decryption with a 128-bit key (16 bytes * 8 = 128 bits).
    mbedtls_aes_setkey_dec(&aes, AES_KEY, 128);

    // Perform the CBC decryption.
    int ret = mbedtls_aes_crypt_cbc(
        &aes,                // AES context
        MBEDTLS_AES_DECRYPT, // Operation: decrypt
        cipher_len,          // Length of the ciphertext in bytes
        iv_copy,             // IV buffer (will be updated internally)
        cipher,              // Input: ciphertext bytes
        plain                // Output: plaintext bytes
    );

    mbedtls_aes_free(&aes);

    // just in case
    if (ret != 0) 
    {
        Serial.print("[AES] decrypt failed, ret=");
        Serial.println(ret);
        free(plain);
        free(decoded);
        return String("");
    }

    // 4) Remove zero padding and convert the plaintext bytes into an Arduino String.
    size_t plain_len = cipher_len;
    while (plain_len > 0 && plain[plain_len - 1] == 0) 
    {
        plain_len--;
    }

    String result;
    result.reserve(plain_len);
    // Append each plaintext byte as a character into the String.
    for (size_t i = 0; i < plain_len; i++) 
    {
        result += (char)plain[i];
    }

    free(plain);
    free(decoded);

    return result;
}

String Hmac_generate(const uint8_t* data, size_t data_len) {
    uint8_t hmac_output[32]; // SHA256 produces 32 bytes
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);    // Algo : SHA256
    
    mbedtls_md_hmac(md_info, AES_KEY, sizeof(AES_KEY), data, data_len, hmac_output);    // Hashed generated - 32 bytes

    // Base64 encode HMAC for sending over MQTT
    return text_encoder(hmac_output, sizeof(hmac_output));
}

String Hmac_encrypt(String payload){

    size_t payload_len = payload.length();
    uint8_t* payload_byte = (uint8_t*) payload.c_str();

    String Hmac_message = Hmac_generate(payload_byte,payload_len);

    return Hmac_message;
}

// Function to verify if the received HMAC matches the calculated HMAC for the payload
bool verify_HMAC(String payload, String received_hmac) {
    // Generate HMAC for the received payload using the shared key (inside Hmac_encrypt)
    String calculated_hmac = Hmac_encrypt(payload);
    
    // Compare the calculated HMAC with the received HMAC
    return calculated_hmac.equals(received_hmac);
}
