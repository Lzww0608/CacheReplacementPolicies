#ifndef UTILS_RAND_H
#define UTILS_RAND_H

#include <random>

namespace CRP {

uint64_t getRandomInt64(uint64_t min, uint64_t max);

uint32_t getRandomInt32(uint32_t min, uint32_t max);

uint16_t getRandomInt16(uint16_t min, uint16_t max);

uint8_t getRandomInt8(uint8_t min, uint8_t max);

double getRandomDouble(double min, double max);

float getRandomFloat(float min, float max);

bool getRandomBool();

bool getRandomProbability(double probability);

}
#endif

