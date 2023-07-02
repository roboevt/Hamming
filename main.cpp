// g++ -Wall -Wextra -std=c++20 -O3 -o hamming hamming.cpp main.cpp

#include <algorithm>
#include <chrono>
#include <vector>

#include "hamming.h"

int main() {
    demo();

    speedTest();

    fileSpeedTest("2mbtest.txt");

    return 0;
}