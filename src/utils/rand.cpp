#include "../../include/utils/rand.h"
#include <random>
#include <chrono>

namespace CRP {

// 全局随机数生成器，使用当前时间作为种子
static std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());

uint64_t getRandomInt64(uint64_t min, uint64_t max) {
    std::uniform_int_distribution<uint64_t> dist(min, max);
    return dist(rng);
}

uint32_t getRandomInt32(uint32_t min, uint32_t max) {
    std::uniform_int_distribution<uint32_t> dist(min, max);
    return dist(rng);
}

uint16_t getRandomInt16(uint16_t min, uint16_t max) {
    std::uniform_int_distribution<uint16_t> dist(min, max);
    return dist(rng);
}

uint8_t getRandomInt8(uint8_t min, uint8_t max) {
    std::uniform_int_distribution<uint8_t> dist(min, max);
    return dist(rng);
}

double getRandomDouble(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(rng);
}

float getRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

bool getRandomBool() {
    std::uniform_int_distribution<int> dist(0, 1);
    return dist(rng) == 1;
}

bool getRandomProbability(double probability) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng) * 100 < probability;
}

}