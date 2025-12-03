#pragma once

#ifndef TESTING
#include <Arduino.h>
#include "aes.h"
#endif

// Function to verify if the received HMAC matches the calculated HMAC for the payload
bool verify_HMAC(String payload, String received_hmac) {
    // Generate HMAC for the received payload using the shared key (inside Hmac_encrypt)
    String calculated_hmac = Hmac_encrypt(payload);
    
    // Compare the calculated HMAC with the received HMAC
    return calculated_hmac.equals(received_hmac);
}
