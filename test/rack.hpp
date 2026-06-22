// Test-only shim: minimal rack.hpp so dsp headers that #include <rack.hpp> compile
// standalone (SpreadInterp.hpp → PatternEngine.hpp). Production uses the real SDK header.
#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
namespace rack {
    struct Module;   // fwd — SpreadInterp::monoBuf takes a const Module* (unused)
    namespace math {
        inline float clamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
        inline int   clamp(int   x, int   lo, int   hi) { return x < lo ? lo : (x > hi ? hi : x); }
    }
    namespace random {
        inline uint64_t u64() { return 0x9E3779B97F4A7C15ull; }  // deterministic for tests
    }
}
