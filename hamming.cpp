#include <iostream>
#include <numeric>
#include <sstream>
#include <bitset>

namespace hamming {
/*
31,26 hamming code (83.9% data)

data:   |a|b|c|d|e|f|g|h|i|j|k|

result: |x|p|p|a|p|b|c|d|p|e|f|g|h|i|j|k|

p = group parity
x = total parity
*/

/// @brief Expand input to leave space for parity bits
/// @param data An at most 26 bit number
/// @return A 31 bit number with 0s in the parity locations.
uint32_t expand(uint32_t data) {
    if (data > (1 << 26)) {
        std::cout << "Message does not fit within data bits" << std::endl;
        return -1;  // ~0
    }
    uint32_t result = 0;

    result |= (data & 0b00000000000000000000000000000001) << 3;
    result |= (data & 0b00000000000000000000000000001110) << 4;
    result |= (data & 0b00000000000000000000011111110000) << 5;
    result |= (data & 0b00000011111111111111100000000000) << 6;

    return result;
}

/// @brief Check a number and return which parity bits should be set
/// @param data Input to analyze
/// @return Which parity bits should be set
uint32_t check(uint32_t data) {
    uint32_t result = 0;
    for (int i = 0; i < sizeof(uint32_t) * __CHAR_BIT__; i++) {
        if (data & 1) {
            result ^= i;
        }
        data >>= 1;
    }
    return result;
}

/// @brief Encode an up to 26 bit number using a 31,26 hamming code
/// @param data Input message
/// @return Encoded result
uint32_t encode(uint32_t message) {
    uint32_t data = expand(message);
    uint32_t correction = check(data);

    for(int i = 0; i < 5; i++) {
        if (correction & 1) {
            data ^= 1 << (1 << i);
        }
        correction >>= 1;
    }
    return data;
}

/// @brief Remove parity bits and return just the message
/// @param data Decoded message with parity bits
/// @return 26 bit original message
uint32_t compress(uint32_t data) {
    uint32_t result = 0;

    result |= (data & 0b00000000000000000000000000001000) >> 3;
    result |= (data & 0b00000000000000000000000011100000) >> 4;
    result |= (data & 0b00000000000000001111111000000000) >> 5;
    result |= (data & 0b11111111111111100000000000000000) >> 6;

    return result;
}

/// @brief Check if any detectable errors were found in a received message and correct them
/// @param data Received data
/// @return Error corrected data with parity bits still present
uint32_t decode(uint32_t data) {
    uint32_t correction = check(data);
    if(correction) {
        std::cout << "Input requires correction, bit " << correction 
                  << " was flipped." << std::endl;
    
        data ^= (1 << correction);
    }
    return data;
}
}  // namespace hamming

uint32_t hammingEncode(uint32_t message) {
    return hamming::encode(hamming::expand(message));
}

uint32_t hammingDecode(uint32_t message) {
    return hamming::compress(hamming::decode(message));
}

template <typename T>
static std::string toBinaryString(const T& x) {
    std::stringstream ss;
    ss << std::bitset<sizeof(T) * 8>(x);
    return ss.str();
}

auto main() -> int {
    srand(time(0));

    uint32_t message = rand() >> ((sizeof(uint32_t) * __CHAR_BIT__) - 26);
    uint32_t expand = hamming::expand(message);
    uint32_t correction = hamming::check(expand);
    uint32_t encoded = hamming::encode(message);

    // Simulate random single bit error
    encoded ^= (1 << rand() % (sizeof(uint32_t) * __CHAR_BIT__));

    uint32_t check = hamming::check(encoded);
    uint32_t decoded = hamming::decode(encoded);
    uint32_t received = hamming::compress(decoded);

    std::cout << "Message:\t" << toBinaryString(message)
              << "\nExpand:\t\t" << toBinaryString(expand)
              << "\nCorrection:\t" << toBinaryString(correction)
              << "\nEncoded:\t" << toBinaryString(encoded)
              << "\nCheck:\t\t" << check
              << "\nDecoded:\t" << toBinaryString(decoded)
              << "\nReceived:\t" << toBinaryString(received)
              << "\nMessage received " << (message == received ? 
                    "successfully!" : "UNsuccessfully :(") << std::endl;
}