#include "hamming.h"

#include <algorithm>
#include <bit>  // popcnt
#include <chrono>
#include <fstream>
#include <vector>

namespace {

constexpr uint32_t LOWER_26 = (1 << 26) - 1;

/// @brief Expand input to leave space for parity bits
/// @param data An at most 26 bit number
/// @return A 31 bit number with 0s in the parity locations.
uint32_t expand(uint32_t data) {
    if (data > (1 << 26)) {
        std::cout << "Message does not fit within data bits" << std::endl;
        return 0;
    }
    uint32_t result = 0;

    result |= (data & 0b00000000000000000000000001) << 3;
    result |= (data & 0b00000000000000000000001110) << 4;
    result |= (data & 0b00000000000000011111110000) << 5;
    result |= (data & 0b11111111111111100000000000) << 6;

    return result;
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
}  // anonymous namespace

namespace hamming {

uint32_t check(uint32_t data) {
    uint32_t result = 0;
    for (uint64_t i = 0; i < sizeof(uint32_t) * __CHAR_BIT__; i++) {
        if (data & 1) {
            result ^= i;
        }
        data >>= 1;
    }
    return result;
}

uint32_t encode(uint32_t message) {
    uint32_t data = expand(message);
    uint32_t correction = check(data);

    for (int i = 0; i < 5; i++) {
        if (correction & 1) {
            data ^= 1 << (1 << i);
        }
        correction >>= 1;
    }

    // Extended code
    uint_fast8_t parity = std::__popcount(data);
    data |= parity & 1;  // ensure parity is even

    return data;
}

uint32_t decode(uint32_t data) {
    uint_fast8_t parity = std::__popcount(data);
    uint32_t correction = check(data);
    if (correction) {
        // std::cout << "Input requires correction, bit " << correction
        //           << " was flipped." << std::endl;
        if (!(parity & 1)) {
            std::cout << "At least 2 bits flipped, unable to decode" << std::endl;
            return 0;
        }
        data ^= (1 << correction);
    }
    return compress(data);
}

uint64_t check(uint64_t message) {
    uint64_t result = 0;
    for (uint64_t i = 0; i < sizeof(uint64_t) * __CHAR_BIT__; i++) {
        if (message & 1) {
            result ^= i;
        }
        message >>= 1;
    }
    return result;
}

uint64_t encode64(uint64_t message) {
    if (message > (static_cast<uint64_t>(1) << 57)) {
        std::cout << "Message does not fit within data bits" << std::endl;
        return 0;
    }
    // Make room in input message for parity bits
    uint64_t expanded = 0;
    expanded |= (message & 0b000000000000000000000000000000000000000000000000000000001) << 3;
    expanded |= (message & 0b000000000000000000000000000000000000000000000000000001110) << 4;
    expanded |= (message & 0b000000000000000000000000000000000000000000000011111110000) << 5;
    expanded |= (message & 0b000000000000000000000000000000011111111111111100000000000) << 6;
    expanded |= (message & 0b111111111111111111111111111111100000000000000000000000000) << 7;

    // Figure out which parity bits need to be set
    uint64_t correction = check(expanded);

    // Set required parity bits
    for (int i = 0; i < 6; i++) {
        if (correction & 1) {
            expanded ^= static_cast<uint64_t>(1) << (1 << i);  // Set the appropriate power-of-two parity bit
        }
        correction >>= 1;
    }

    // Extended code
    uint_fast8_t parity = std::__popcount(expanded);
    expanded |= parity & 1;  // ensure parity is even

    return expanded;
}

uint64_t decode64(uint64_t message) {
    uint64_t correction = check(message);
    uint_fast8_t parity = std::__popcount(message);
    if (correction) {
        // std::cout << "Input requires correction, bit " << correction
        //           << " was flipped." << std::endl;
        if (!(parity & 1)) {
            std::cout << "At least 2 bits flipped, unable to decode" << std::endl;
            return 0;
        }
        message ^= (static_cast<uint64_t>(1) << correction);
    }

    uint64_t compressed = 0;
    compressed |= (message & 0b0000000000000000000000000000000000000000000000000000000000001000) >> 3;
    compressed |= (message & 0b0000000000000000000000000000000000000000000000000000000011100000) >> 4;
    compressed |= (message & 0b0000000000000000000000000000000000000000000000001111111000000000) >> 5;
    compressed |= (message & 0b0000000000000000000000000000000011111111111111100000000000000000) >> 6;
    compressed |= (message & 0b1111111111111111111111111111111000000000000000000000000000000000) >> 7;

    return compressed;
}

void encodeFile(const std::string& input, const std::string& output) {
    std::ifstream in(input, std::ios::binary);
    std::ofstream out(output, std::ios::binary);

    if (!in.is_open()) {
        std::cout << "Unable to open input file" << std::endl;
        return;
    }
    if (!out.is_open()) {
        std::cout << "Unable to open output file" << std::endl;
        return;
    }

    // LCM(26, 32) == 416 bits, which is 16 26-bit words and 13 32-bit words
    std::array<uint32_t, 13> inBuffer;
    std::array<uint32_t, 16> outBuffer;

    // Read in 13 32-bit words at a time
    while (in.read(reinterpret_cast<char*>(inBuffer.data()), sizeof(uint32_t) * inBuffer.size())) {
        int currentBit = 0;

        uint64_t i = 0;
        uint64_t o = 0;

        uint32_t firstBlock = inBuffer[i++];
        while(o < outBuffer.size()) {
            int startBit = currentBit % (sizeof(uint32_t) * __CHAR_BIT__);
            int endBit = startBit + 26;

            uint32_t message;
            if (endBit < 32) {
                message = (firstBlock >> startBit) & LOWER_26;
            } else {
                uint32_t secondBlock = inBuffer[i++];
                message = (((firstBlock >> startBit) & LOWER_26) | (secondBlock << (32 - startBit))) & LOWER_26;
                firstBlock = secondBlock;
            }
            outBuffer[o++] = encode(message);
            currentBit += 26;
        }

        out.write(reinterpret_cast<char*>(outBuffer.data()), sizeof(uint32_t) * outBuffer.size());
    }
    in.close();
    out.close();
}

void decodeFile(const std::string& input, const std::string& output) {
    std::ifstream in(input, std::ios::binary);
    std::ofstream out(output, std::ios::binary);

    if (!in.is_open()) {
        std::cout << "Unable to open input file" << std::endl;
        return;
    }
    if (!out.is_open()) {
        std::cout << "Unable to open output file" << std::endl;
        return;
    }

    std::array<uint32_t, 16> inBuffer;
    std::array<uint32_t, 13> outBuffer;

    // Read in 13 32-bit words at a time
    while (in.read(reinterpret_cast<char*>(inBuffer.data()), sizeof(uint32_t) * inBuffer.size())) {
        outBuffer.fill(0);

        int currentBit = 0;

        uint64_t i = 0;
        uint64_t o = 0;

        while(i < inBuffer.size()) {
            int startBit = currentBit % (sizeof(uint32_t) * __CHAR_BIT__);
            int endBit = startBit + 26;

            uint32_t message = decode(inBuffer[i]);

            if (endBit < 32) {
                outBuffer[o] |= message << (6 - startBit);
            } else {
                outBuffer[o++] |= message << startBit;
                outBuffer[o] |= message >> (32 - startBit);
            }
            currentBit += 26;
            i++;
        }

        out.write(reinterpret_cast<char*>(outBuffer.data()), sizeof(uint32_t) * outBuffer.size());
    }
    in.close();
    out.clear();
}


}  // namespace hamming

void demo() {
    srand(time(0));

    std::cout << "--------------------Demo--------------------" << std::endl
              << std::endl;

    uint32_t message = rand() >> ((sizeof(uint32_t) * __CHAR_BIT__) - 26);
    uint32_t expanded = expand(message);
    uint32_t correction = hamming::check(expanded);
    uint32_t encoded = hamming::encode(message);

    // Simulate random single bit error
    encoded ^= (1 << rand() % (sizeof(uint32_t) * __CHAR_BIT__));

    uint32_t check = hamming::check(encoded);
    uint32_t received = hamming::decode(encoded);

    std::cout << "Message:\t" << toBinaryString(message)
              << "\nExpanded:\t" << toBinaryString(expanded)
              << "\nCorrection:\t" << toBinaryString(correction)
              << "\nEncoded:\t" << toBinaryString(encoded)
              << "\nCheck:\t\t" << check
              << "\nReceived:\t" << toBinaryString(received)
              << "\nMessage received " << (message == received ? "successfully!" : "UNsuccessfully :(") << std::endl;

    uint64_t message64 = ((uint64_t)rand() | (uint64_t)rand() << 32) >> ((sizeof(uint64_t) * __CHAR_BIT__) - 57);
    uint64_t encoded64 = hamming::encode64(message64);

    // Simulate random single bit error
    encoded64 ^= (1 << rand() % (sizeof(uint32_t) * __CHAR_BIT__));

    uint64_t decoded64 = hamming::decode64(encoded64);

    std::cout << "\n64 Bit:\nMessage:\t" << toBinaryString(message64)
              << "\nEncoded:\t" << toBinaryString(encoded64)

              << "\nDecoded:\t" << toBinaryString(decoded64)

              << "\nMessage received " << (message64 == decoded64 ? "successfully!" : "UNsuccessfully :(") << std::endl;
}

void speedTest(int power) {
    srand(time(0));

    std::cout << "\n-----------------Speed Test-----------------" << std::endl;

    ssize_t len = 1 << power;
    std::cout << "Testing with " << len / 1e3 << "k integers\n"
              << std::endl;

    std::vector<uint32_t> data(len);
    std::generate(data.begin(), data.end(), []() {
        return rand() >> ((sizeof(uint32_t) * __CHAR_BIT__) - 26);
    });

    std::vector<uint32_t> encoded(len);

    auto start = std::chrono::high_resolution_clock::now();

    // Encode data
    std::transform(data.begin(), data.end(), encoded.begin(), hamming::encode);

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Encoding speed: " << sizeof(uint32_t) * len / 1e6 / elapsed.count()
              << " MB/s" << std::endl;

    std::vector<uint32_t> decoded(len);

    start = std::chrono::high_resolution_clock::now();

    // Decode data
    std::transform(encoded.begin(), encoded.end(), decoded.begin(), hamming::decode);

    end = std::chrono::high_resolution_clock::now();

    elapsed = end - start;

    std::cout << "Decoding speed: " << sizeof(uint32_t) * len / 1e6 / elapsed.count()
              << " MB/s" << std::endl;

    // Corrupt data
    std::transform(encoded.begin(), encoded.end(), encoded.begin(), [](uint32_t data) {
        return data ^ (1 << (rand() % 26));
    });

    start = std::chrono::high_resolution_clock::now();

    // Decode corrupted data
    std::transform(encoded.begin(), encoded.end(), decoded.begin(), hamming::decode);

    end = std::chrono::high_resolution_clock::now();

    elapsed = end - start;

    std::cout << "Corrupted decoding speed: " << sizeof(uint32_t) * len / 1e6 / elapsed.count()
              << " MB/s" << std::endl;

    std::vector<uint64_t> data64(len);
    std::generate(data64.begin(), data64.end(), []() {
        return ((uint64_t)rand() | (uint64_t)rand() << 32) >> ((sizeof(uint64_t) * __CHAR_BIT__) - 57);
    });

    // 64 Bit data

    std::vector<uint64_t> encoded64(len);

    start = std::chrono::high_resolution_clock::now();

    // Encode data
    std::transform(data64.begin(), data64.end(), encoded64.begin(), hamming::encode64);

    end = std::chrono::high_resolution_clock::now();

    elapsed = end - start;

    std::cout << "\n64 Bit:\nEncoding speed: " << sizeof(uint64_t) * len / 1e6 / elapsed.count()
              << " MB/s" << std::endl;

    std::vector<uint64_t> decoded64(len);

    start = std::chrono::high_resolution_clock::now();

    // Decode data
    std::transform(encoded64.begin(), encoded64.end(), decoded64.begin(), hamming::decode64);

    end = std::chrono::high_resolution_clock::now();

    elapsed = end - start;

    std::cout << "Decoding speed: " << sizeof(uint64_t) * len / 1e6 / elapsed.count()
              << " MB/s" << std::endl;

    // Corrupt data
    std::transform(encoded64.begin(), encoded64.end(), encoded64.begin(), [](uint64_t data) {
        return data ^ (1 << (rand() % 57));
    });

    start = std::chrono::high_resolution_clock::now();

    // Decode corrupted data
    std::transform(encoded64.begin(), encoded64.end(), decoded64.begin(), hamming::decode64);

    end = std::chrono::high_resolution_clock::now();

    elapsed = end - start;

    std::cout << "Corrupted decoding speed: " << sizeof(uint64_t) * len / 1e6 / elapsed.count()
              << " MB/s" << std::endl;
}