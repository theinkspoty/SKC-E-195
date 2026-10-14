#pragma once
#include <random>
#include <cmath>

namespace skc {

inline std::mt19937& rng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

inline float frand(float a = 0.f, float b = 1.f) {
    std::uniform_real_distribution<float> d(a, b);
    return d(rng());
}

inline int irand(int a, int b) {
    std::uniform_int_distribution<int> d(a, b);
    return d(rng());
}

inline float gauss(float mean, float stddev) {
    std::normal_distribution<float> d(mean, stddev);
    return d(rng());
}

inline bool chance(float p) {
    return frand() < p;
}

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

}
