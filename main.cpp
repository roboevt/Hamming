// g++ -std=c++20 -O3 -o hamming hamming.cpp main.cpp

#include <algorithm>
#include <chrono>
#include <vector>

#include "hamming.h"

int main() {
    hamming::encodeFile("hamming.cpp", "output.ham");
    std::cout << "Encoded hamming.cpp to output.ham\n";

    hamming::decodeFile("output.ham", "output.cpp");
    std::cout << "Decoded output.ham to output.cpp\n";

    return 0;
}