#pragma once                // Avoid multiple usage of the files
#include <Arduino.h>
#include "Preferences.h"    // Use the flash memory
#include "mbedtls/aes.h"    // AES Accelerator
#include "esp_system.h"     // Random byte generation

Preferences prefs;

void write_key_flash(){

    static const uint8_t AES_KEY [16] = {0x54, 0x68, 0x61, 0x74, 0x73, 0x20, 0x6d, 0x79, 0x20, 0x4b, 0x75, 0x6e, 0x67, 0x20, 0x46, 0x75};
    prefs.begin("Encryption",false);
    prefs.putBytes("Encrypted_AES",AES_KEY,16);
    prefs.end();

}

void load_key_flash(){

    prefs.begin("Encryption",false);
    prefs.getBytes("Encrypted_AES",AES_KEY,16);
    prefs.end();

}

String text_encoder(const uint8_t* data, size_t data_len){

    String output = "";
    size_t output_len = 0;
    size_t base64_len = 4 * ((data_len + 2) / 3);           // Base64 is 33% bigger in memory than byte data - "+2" to ensure ceiling of data/3
    unsigned char* base64 = (unsigned char*) malloc(base64_len);

    MBEDTLS_BASE64_C(base64,base64_len,&output_len,data,data_len);      // Convert binary to C string

    for (int i=0;i<output_len;i++){
        output += (char)base64[i];      // Make a Arduino String
    }

    free(base64);
    return output;
}

String aes_encrypt(const String& plaintext){

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    String aes_message = "";

    size_t input_len = plaintext.length();
    size_t padded_len = (input_len + 15) & ~15;   // AND logic with 11110000 -> Find the multiple of chunks of 16 bytes

    uint8_t* input_buf = (uint8_t*)calloc(1, padded_len);   // calloc for filling memory with "0"
    memcpy(input_buf, plaintext.c_str(), input_len);

    uint8_t* output_buf = (uint8_t*)calloc(1, padded_len);

    uint8_t AES_IV[16];
    esp_fill_random(AES_IV,16);

    uint8_t aes_IV_copy [16];
    memcpy(aes_IV_copy,AES_IV,16);

    mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_len, aes_IV_copy, input_buf, output_buf);

    size_t final_payload_size = sizeof(aes_IV_copy) + padded_len;
    uint8_t * final_message = (uint8_t *) calloc(1,final_payload_size);

    memcpy(final_message, aes_IV_copy, sizeof(aes_IV_copy) );
    memcpy(final_message + sizeof(aes_IV_copy), output_buf, padded_len);

    aes_message = text_encoder(final_message,final_payload_size);

    free (input_buf);
    free (output_buf);
    free (final_message);
    mbedtls_aes_free(&aes);

    return aes_message;
}