#pragma once
/**
 * test_stubs.hpp
 * Minimal stubs for rack:: types needed by PatternEngine.hpp and GateState.hpp.
 * Include this BEFORE PatternEngine.hpp / GateState.hpp in test files ONLY.
 * Never include in production code — rack.hpp provides the real definitions.
 *
 * Usage in test files:
 *   #include "test_stubs.hpp"   // must come first
 *   #include "PatternEngine.hpp"
 *   #include "GateState.hpp"
 */

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace rack {

namespace random {
    struct Xoroshiro128Plus {
        uint64_t state[2] = {1, 0};
        void seed(uint64_t s0, uint64_t s1) {
            state[0] = s0;
            state[1] = s1 ? s1 : 1;
        }
        uint64_t operator()() {
            uint64_t s0 = state[0], s1 = state[1];
            uint64_t r  = s0 + s1;
            s1 ^= s0;
            state[0] = ((s0 << 55) | (s0 >> 9)) ^ s1 ^ (s1 << 14);
            state[1] = (s1 << 36) | (s1 >> 28);
            return r;
        }
    };

    // Free function used by PatternEngine internals
    inline float uniform() {
        static Xoroshiro128Plus r;
        r.seed(0xdeadbeef, 0xcafe1234);
        return (r() >> 11) * (1.f / float(1ull << 53));
    }
} // namespace random

namespace dsp {
    struct PulseGenerator {
        float remaining = 0.f;
        void  reset()              { remaining = 0.f; }
        void  trigger(float dur)   { if (dur > remaining) remaining = dur; }
        // Matches Rack behaviour: returns true (and decrements) while remaining > 0
        bool  process(float dt)    {
            if (remaining > 0.f) { remaining -= dt; return true; }
            return false;
        }
        bool  isHigh() const       { return remaining > 0.f; }
    };
} // namespace dsp

} // namespace rack
