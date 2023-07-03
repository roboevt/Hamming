#ifndef HAMMING_H
#define HAMMING_H

#include <bitset>
#include <iostream>
#include <sstream>

namespace hamming {
/*
31,26 hamming code (83.9% data)

message:   |a|b|c|d|e|f|g|h|i|j|k|...

result: |x|p|p|a|p|b|c|d|p|e|f|g|h|i|j|k|...

p = group parity
x = total parity
*/

/// @brief Check a number and return which parity bits should be set
/// @param data Input to analyze
/// @return Which parity bits should be set
uint32_t check(uint32_t data);

/// @brief Encode an up to 26 bit number using a 31,26 hamming code
/// @param data Input message
/// @return Encoded result
uint32_t encode(uint32_t message);

/// @brief Check if any detectable errors were found in a received message and correct them
/// @param data Received data
/// @return Error corrected data with parity bits still present
uint32_t decode(uint32_t data);


// 63,57 Hamming code (90.5% data)

uint64_t check(uint64_t message);

uint64_t encode64(uint64_t message);

uint64_t decode64(uint64_t message);

void encodeFile(const std::string& input, const std::string& output);

void decodeFile(const std::string& input, const std::string& output);

}  // namespace hamming

void demo();

void speedTest(int power = 20);

void fileSpeedTest(const std::string& input);

#endif  // HAMMING_H