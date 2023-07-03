// g++ -Wall -Wextra -std=c++20 -O3 -o hamming hamming.cpp main.cpp

#include <algorithm>
#include <chrono>
#include <vector>

#include "hamming.h"

int main() {
    demo();

    test();

    speedTest();

    fileSpeedTest("32mbtest.txt");

    return 0;
}