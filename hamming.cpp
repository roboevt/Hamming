#include "hamming.h"

#include <x86intrin.h>  // pext/pdep

#include <algorithm>
#include <bit>  // popcnt
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>  // digits
#include <vector>

constexpr uint32_t LOWER_26 = (1 << 26) - 1;
constexpr uint64_t LOWER_26L = (1L << 26) - 1;
constexpr uint64_t LOWER_57 = (1L << 57) - 1;
constexpr uint32_t BITS_32 = std::numeric_limits<uint32_t>::digits;

namespace {

template <typename T>
static std::string toBinaryString(const T& x) {
    std::stringstream ss;
    ss << std::bitset<sizeof(T) * 8>(x);
    return ss.str();
}

/// @brief Expand input to leave space for parity bits
/// @param data An at most 26 bit number
/// @return A 31 bit number with 0s in the parity locations.
uint32_t expand(uint32_t data) {
    if (data > (1 << 26)) {
        std::cout << "Message does not fit within data bits" << std::endl;
        return 0;
    }
    uint32_t result = 0;

    // Non pdep version, same functionality
    // result |= (data & 0b00000000000000000000000001) << 3;
    // result |= (data & 0b00000000000000000000001110) << 4;
    // result |= (data & 0b00000000000000011111110000) << 5;
    // result |= (data & 0b11111111111111100000000000) << 6;

    // See here for an explanation of pdep and pext, they are so perfect for this!
    // https://youtube.com/clip/UgkxbDhQnTypipk0PC48W513ezrinnu3DAK2
    result = _pdep_u32(data, 0b11111111111111101111111011101000);

    return result;
}

/// @brief Remove parity bits and return just the message
/// @param data Decoded message with parity bits
/// @return 26 bit original message
uint32_t compress(const uint32_t data) {
    uint32_t result = 0;

    // Non pext version, same functionality
    // result |= (data & 0b00000000000000000000000000001000) >> 3;
    // result |= (data & 0b00000000000000000000000011100000) >> 4;
    // result |= (data & 0b00000000000000001111111000000000) >> 5;
    // result |= (data & 0b11111111111111100000000000000000) >> 6;

    result = _pext_u32(data, 0b11111111111111101111111011101000);
    return result;
}
}  // anonymous namespace

namespace hamming {

uint32_t check(uint32_t data) {
    uint32_t result = 0;
    for (uint64_t i = 0; i < BITS_32; i++) {
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

    bool continueReading = true;
    // Read in 13 32-bit words at a time
    while (continueReading) {
        inBuffer.fill(0);
        in.read(reinterpret_cast<char*>(inBuffer.data()), sizeof(uint32_t) * inBuffer.size());
        if (in.eof()) {
            continueReading = false;
        }
        int currentBit = 0;

        uint64_t i = 0;
        uint64_t o = 0;

        uint32_t firstBlock = inBuffer[i++];

        // Fill outBuffer with 16 26-bit words split from inBuffer
        while (o < outBuffer.size()) {
            int startBit = currentBit % BITS_32;

            // Extract next 26-bit message
            uint32_t message;
            if (startBit + 26 < 32) {  // message is contained within one 32-bit word
                // message = (firstBlock >> startBit) & LOWER_26;
                message = _pext_u32(firstBlock, LOWER_26 << startBit);
            } else {  // message is split between two 32-bit words
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

    // Read in 16 32-bit words at a time
    while (in.read(reinterpret_cast<char*>(inBuffer.data()), sizeof(uint32_t) * inBuffer.size())) {
        outBuffer.fill(0);
        int currentBit = 0;

        uint64_t i = 0;
        uint64_t o = 0;

        // Fill outBuffer with full 32-bit words from 26 bit segments in inBuffer
        while (i < inBuffer.size()) {
            int startBit = currentBit % BITS_32;
            int endBit = startBit + 26;

            uint32_t message = decode(inBuffer[i++]);

            // Write next 26 bits of original message to outBuffer
            if (endBit < 32) {  // message will be contained within one 32-bit word
                outBuffer[o] |= message << startBit;
            } else {  // message will be split between two 32-bit words
                outBuffer[o++] |= message << startBit;
                outBuffer[o] |= message >> (32 - startBit);
            }

            currentBit += 26;
        }

        // Avoid writing 0 bytes at the end of the file
        auto findNonZeroBytes = [](char* array, uint size) {
            uint i = 1;
            while (array[i] != 0 && i < size) {
                i++;
            }
            return i;
        };
        int nonZeroBytes = findNonZeroBytes(reinterpret_cast<char*>(outBuffer.data()), sizeof(uint32_t) * outBuffer.size());

        out.write(reinterpret_cast<char*>(outBuffer.data()), nonZeroBytes);
    }
    in.close();
    out.clear();
}

}  // namespace hamming

void demo() {
    std::cout << "--------------------Demo--------------------" << std::endl
              << std::endl;
    srand(time(0));

    uint32_t message = rand() & LOWER_26;
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
              << std::endl
              << "\nFlipped bit:\t" << check
              << "\nReceived:\t" << toBinaryString(received)
              << "\nMessage received " << (message == received ? "successfully!" : "UNsuccessfully :(") << std::endl;
}

void test() {
    std::cout << "\n--------------------Test--------------------" << std::endl
              << std::endl;

    srand(time(0));

    // Generate random data
    ssize_t len = 1 << 20;
    std::vector<uint32_t> data(len);
    std::generate(data.begin(), data.end(), []() {
        return rand() & LOWER_26;
    });

    // Encode data
    std::vector<uint32_t> encoded(len);
    std::transform(data.begin(), data.end(), encoded.begin(), hamming::encode);

    // Corrupt data
    std::transform(encoded.begin(), encoded.end(), encoded.begin(), [](uint32_t data) {
        return data ^ (1 << (rand() % 26));
    });

    // Decode data
    std::transform(encoded.begin(), encoded.end(), encoded.begin(), hamming::decode);

    // Check for errors
    if (std::equal(data.begin(), data.end(), encoded.begin())) {
        std::cout << "Test passed! All decoded elements match." << std::endl;
    } else {
        std::cout << "Test failed :(" << std::endl;

        // Find first unequal element
        auto mismatch = std::mismatch(data.begin(), data.end(), encoded.begin());
        std::cout << "Mismatch at index " << std::distance(data.begin(), mismatch.first) << std::endl;
        std::cout << "Expected: " << toBinaryString(*mismatch.first) << ", ";
        std::cout << "Received: " << toBinaryString(*mismatch.second) << std::endl;
    }
}

void speedTest(int power) {
    std::cout << "\n-----------------Speed Test-----------------" << std::endl;

    // Setup
    srand(time(0));
    ssize_t len = 1 << power;
    std::cout << "Testing with " << len / 1e3 << "k integers\n"
              << std::endl;

    // Generate random test data
    std::vector<uint32_t> data(len);
    std::generate(data.begin(), data.end(), []() {
        return rand() >> ((sizeof(uint32_t) * __CHAR_BIT__) - 26);
    });

    // Encode data
    std::vector<uint32_t> encoded(len);
    auto start = std::chrono::high_resolution_clock::now();
    std::transform(data.begin(), data.end(), encoded.begin(), hamming::encode);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Encoding speed: " << sizeof(uint32_t) * len / 1e6 / elapsed.count()
              << " MB/s" << std::endl;

    // Decode data
    std::vector<uint32_t> decoded(len);
    start = std::chrono::high_resolution_clock::now();
    std::transform(encoded.begin(), encoded.end(), decoded.begin(), hamming::decode);
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Decoding speed: " << sizeof(uint32_t) * len / 1e6 / elapsed.count()
              << " MB/s" << std::endl;

    // Corrupt data
    std::transform(encoded.begin(), encoded.end(), encoded.begin(), [](uint32_t data) {
        return data ^ (1 << (rand() % 26));
    });

    // Decode corrupted data
    start = std::chrono::high_resolution_clock::now();
    std::transform(encoded.begin(), encoded.end(), decoded.begin(), hamming::decode);
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Corrupted decoding speed: " << sizeof(uint32_t) * len / 1e6 / elapsed.count()
              << " MB/s" << std::endl;
}

void fileSpeedTest(const std::string& input) {
    std::cout << "\n---------------File Speed Test--------------" << std::endl;

    std::filesystem::path inputPath(input);
    if (!std::filesystem::exists(inputPath)) {
        std::cout << "File " << input << " does not exist!" << std::endl;
        return;
    }
    int fileSize = std::filesystem::file_size(input);
    std::cout << "Testing with " << fileSize / 1e6 << " MB file\n"
              << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    hamming::encodeFile(input, "encoded.ham");
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Encoding speed: " << fileSize / 1e6 / elapsed.count()
              << " MB/s" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    hamming::decodeFile("encoded.ham", "output.txt");
    end = std::chrono::high_resolution_clock::now();

    elapsed = end - start;
    std::cout << "Decoding speed: " << fileSize / 1e6 / elapsed.count()
              << " MB/s" << std::endl;
}