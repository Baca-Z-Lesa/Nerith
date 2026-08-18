#include "core/Random.h"

#include <random>

namespace
{
    std::mt19937& generator()
    {
        static std::mt19937 engine{ std::random_device{}() };
        return engine;
    }
}

float rng::range(float min, float max)
{
    return std::uniform_real_distribution<float>(min, max)(generator());
}
