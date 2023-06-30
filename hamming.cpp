#include <bitset>
#include <iostream>
#include <numeric>
#include <sstream>
#include <vector>

using namespace std;

/*
31,26 hamming code (83.9% data)

data:   |a|b|c|d|e|f|g|h|i|j|k|

result: |x|p|p|a|p|b|c|d|p|e|f|g|h|i|j|k|

p = group parity
x = total parity
*/

uint32_t setupData(uint32_t data) {
    if (data > (1 << 26)) {
        cout << "Message does not fit within data bits" << endl;
        return -1;  // ~0
    }
    uint32_t result = 0;

    result |= (data & 1) << 3;
    result |= (data & 0b0111) << 5;
    result |= (data & 0b00001111111) << 9;
    result |= (data & 0b00000000000111111111111111) << 17;

    return result;
}

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

uint32_t encode(uint32_t data) {
    uint32_t correction = check(data);

    if(correction) {
        data ^= (1 << correction);
    }
    return data;
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
    uint32_t data = setupData(message);
    uint32_t correction = check(data);
    uint32_t result = encode(data);
    uint32_t zero = check(result);
    std::cout << "Message:\t" << toBinaryString(message)
              << "\nData:\t\t" << toBinaryString(data) 
              << "\nCorrection:\t" << toBinaryString(correction) 
              << "\nResult:\t\t" << toBinaryString(result) 
              << "\nZero:\t\t" << toBinaryString(zero) << std::endl;
}