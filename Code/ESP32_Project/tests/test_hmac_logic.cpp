#include <iostream>
#include <string>
#include <vector>

// Define TESTING to avoid including Arduino.h and aes.h in hmac_verification.h
#define TESTING

// Mock Arduino String class
class String {
public:
    std::string s;
    String(const char* str) : s(str) {}
    String(std::string str) : s(str) {}
    String() : s("") {}
    
    bool equals(const String& other) const {
        return s == other.s;
    }
    
    const char* c_str() const {
        return s.c_str();
    }
    
    // Operator == for comparison
    bool operator==(const String& other) const {
        return s == other.s;
    }
};

// Mock Hmac_encrypt function
// In reality, this would use the AES key. For testing, we'll just return a predictable "hash".
// In here we are rewritting the Hmac_encrypt function inside the verify_HMAC function
String Hmac_encrypt(String payload) {
    return String("HMAC_" + payload.s);
}

// Include the file to be tested
#include "../src/hmac_verification.h"

int main() {
    std::cout << "Running HMAC Verification Tests..." << std::endl;
    
    // Test Case 1: Valid HMAC
    String payload1 = "Temperature:25.5";
    String valid_hmac1 = "HMAC_Temperature:25.5";
    
    if (verify_HMAC(payload1, valid_hmac1)) {
        std::cout << "[PASS] Test Case 1: Valid HMAC verified successfully." << std::endl;
    } else {
        std::cout << "[FAIL] Test Case 1: Valid HMAC failed verification." << std::endl;
        return 1;
    }
    
    // Test Case 2: Invalid HMAC
    String payload2 = "Humidity:60";
    String invalid_hmac2 = "HMAC_Temperature:25.5"; // Wrong HMAC
    
    if (!verify_HMAC(payload2, invalid_hmac2)) {
        std::cout << "[PASS] Test Case 2: Invalid HMAC rejected successfully." << std::endl;
    } else {
        std::cout << "[FAIL] Test Case 2: Invalid HMAC was incorrectly accepted." << std::endl;
        return 1;
    }
    
    // Test Case 3: Tampered Payload
    String payload3 = "Temperature:25.5";
    String tampered_hmac3 = "HMAC_Temperature:25.6"; // Slightly different
    
    if (!verify_HMAC(payload3, tampered_hmac3)) {
        std::cout << "[PASS] Test Case 3: Tampered HMAC rejected successfully." << std::endl;
    } else {
        std::cout << "[FAIL] Test Case 3: Tampered HMAC was incorrectly accepted." << std::endl;
        return 1;
    }

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
