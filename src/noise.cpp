#include "noise.h"

namespace simlib::noise {

Generator::Generator(int seed)
    : generator_(seed) {
}

void Generator::set_frequency(float frequency) {
    generator_.SetFrequency(frequency);
}

void Generator::set_type(FastNoiseLite::NoiseType type) {
    generator_.SetNoiseType(type);
}

float Generator::get(float x, float y) const {
    return generator_.GetNoise(x, y);
}

float Generator::get(float x, float y, float z) const {
    return generator_.GetNoise(x, y, z);
}

} // namespace simlib::noise
