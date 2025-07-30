#include "include/utils/rand.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "Testing CRP Random Number Generator Functions\n";
    std::cout << "=============================================\n\n";
    
    // Test integer functions
    std::cout << "Integer Functions:\n";
    std::cout << "getRandomInt64(1, 100): " << CRP::getRandomInt64(1, 100) << "\n";
    std::cout << "getRandomInt32(1, 100): " << CRP::getRandomInt32(1, 100) << "\n";
    std::cout << "getRandomInt16(1, 100): " << CRP::getRandomInt16(1, 100) << "\n";
    std::cout << "getRandomInt8(1, 100): " << (int)CRP::getRandomInt8(1, 100) << "\n\n";
    
    // Test floating point functions
    std::cout << "Floating Point Functions:\n";
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "getRandomDouble(0.0, 1.0): " << CRP::getRandomDouble(0.0, 1.0) << "\n";
    std::cout << "getRandomFloat(0.0f, 1.0f): " << CRP::getRandomFloat(0.0f, 1.0f) << "\n\n";
    
    // Test boolean functions
    std::cout << "Boolean Functions:\n";
    std::cout << "getRandomBool(): " << (CRP::getRandomBool() ? "true" : "false") << "\n";
    std::cout << "getRandomProbability(0.3): " << (CRP::getRandomProbability(0.3) ? "true" : "false") << "\n";
    std::cout << "getRandomProbability(0.7): " << (CRP::getRandomProbability(0.7) ? "true" : "false") << "\n\n";
    
    // Test multiple calls to show randomness
    std::cout << "Multiple calls to show randomness:\n";
    for (int i = 0; i < 10; ++i) {
        std::cout << CRP::getRandomInt32(1, 10) << " ";
    }
    std::cout << "\n";
    
    return 0;
} 