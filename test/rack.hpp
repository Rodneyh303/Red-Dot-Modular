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
        inline uint32_t u32() { return 0x9E3779B9u; }            // deterministic for tests
    }
    // Minimal deterministic DSP stubs — enough for the engine to CONSTRUCT in unit tests.
    // Tests that exercise the resolver's read accessors don't drive the clock/gate, so the
    // behaviour here only needs to be well-defined, not Rack-accurate.
    namespace dsp {
        struct SchmittTrigger {
            bool state = false;
            void reset() { state = false; }
            bool process(float in, float lo = 0.1f, float hi = 1.f) {
                if (state) { if (in <= lo) state = false; return false; }
                else       { if (in >= hi) { state = true; return true; } return false; }
            }
            bool isHigh() const { return state; }
        };
        struct PulseGenerator {
            float remaining = 0.f;
            void  trigger(float dur = 1e-3f) { if (dur > remaining) remaining = dur; }
            bool  process(float dt) { if (remaining > 0.f) { remaining -= dt; return true; } return false; }
            void  reset() { remaining = 0.f; }
        };
    }
}
