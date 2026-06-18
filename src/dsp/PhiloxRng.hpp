#pragma once
/**
 * PhiloxRng.hpp
 * Philox4x32-10 — the counter-based PRNG from Salmon et al., "Parallel Random
 * Numbers: As Easy as 1, 2, 3" (Random123). Same family C++26 adds as
 * std::philox_engine; this is a standalone C++17 implementation of the identical
 * algorithm (verified against the canonical Random123 known-answer test vectors).
 *
 * WHY (same rationale as SquaresRng): counter-based ⇒ STATELESS + ADDRESSABLE.
 * philox4x32(counter, key) is a pure function — the draw at index N doesn't depend
 * on having generated 0..N-1, so the stream is reversible / random-access. That's
 * what a phase-driven reverse/scrub mode needs (replay draws backwards within one
 * key; a new key = new sequence). Philox is the better-pedigreed of the two
 * (NumPy/TensorFlow default), and yields 4 x 32-bit outputs per block, which suits
 * block-drawing the pattern arrays.
 *
 * Exposes the same shape as SquaresRng so either can drop into PatternEngine:
 *   - seed()/next()/nextFloat() sequential facade
 *   - at(counter)/atUniform(counter) addressable core
 *   - fillBlock()/drawBlock() block drawing
 *
 * Header-only, no Rack dependency.
 */

#include <cstdint>
#include <cstring>
#include <array>

namespace redDot {

// ── Philox4x32-10 core (verbatim algorithm, reference form) ──────────────────
// 4 x 32-bit counter, 2 x 32-bit key, 10 rounds. Returns 4 x 32-bit output that is
// a pure function of (counter, key).
inline std::array<uint32_t,4> philox4x32_10(std::array<uint32_t,4> ctr,
                                            std::array<uint32_t,2> key) {
    // Canonical Random123 constants (verified against DEShawResearch/random123
    // include/Random123/philox.h).
    constexpr uint32_t M0 = 0xD2511F53u;   // PHILOX_M4x32_0
    constexpr uint32_t M1 = 0xCD9E8D57u;   // PHILOX_M4x32_1
    constexpr uint32_t W0 = 0x9E3779B9u;   // PHILOX_W32_0 (golden ratio)
    constexpr uint32_t W1 = 0xBB67AE85u;   // PHILOX_W32_1 (sqrt(3)-1)

    auto round = [](std::array<uint32_t,4> c, std::array<uint32_t,2> k) {
        uint64_t p0 = (uint64_t)M0 * c[0];   // lo0 = p0, hi0 = p0>>32
        uint64_t p1 = (uint64_t)M1 * c[2];   // lo1 = p1, hi1 = p1>>32
        uint32_t hi0 = (uint32_t)(p0 >> 32), lo0 = (uint32_t)p0;
        uint32_t hi1 = (uint32_t)(p1 >> 32), lo1 = (uint32_t)p1;
        return std::array<uint32_t,4>{
            (uint32_t)(hi1 ^ c[1] ^ k[0]), lo1,
            (uint32_t)(hi0 ^ c[3] ^ k[1]), lo0 };
    };
    auto bump = [](std::array<uint32_t,2> k) {
        return std::array<uint32_t,2>{ (uint32_t)(k[0] + W0), (uint32_t)(k[1] + W1) };
    };

    // Canonical schedule: round 0 uses the raw key; each subsequent round bumps the
    // key BEFORE applying it. 10 rounds ⇒ 1 raw + 9 (bump,round) pairs.
    ctr = round(ctr, key);
    for (int r = 1; r < 10; ++r) { key = bump(key); ctr = round(ctr, key); }
    return ctr;
}

// ── Uniform [0,1) from 32 random bits ────────────────────────────────────────
// 24-bit mantissa precision (single-precision-clean), matches typical use.
inline float philoxUniformFloat(uint32_t bits) {
    // top 24 bits → [0,1); 2^-24 step.
    return (bits >> 8) * (1.0f / 16777216.0f);
}
// Full 53-bit double uniform from two 32-bit words.
inline double philoxUniformDouble(uint32_t hi, uint32_t lo) {
    uint64_t v = ((uint64_t)hi << 32) | lo;
    constexpr uint64_t exp = 0x3FF0000000000000ULL;
    constexpr uint64_t man = 0x000FFFFFFFFFFFFFULL;
    uint64_t mapped = exp | (v & man);
    double d; std::memcpy(&d, &mapped, sizeof(double));
    return d - 1.0;
}

// ── Key conditioning (so a weak seed, e.g. a 0..10V CV, still mixes well) ─────
inline std::array<uint32_t,2> philoxMakeKey(uint64_t seed) {
    uint64_t z = seed + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z =  z ^ (z >> 31);
    return { (uint32_t)(z & 0xFFFFFFFFu), (uint32_t)(z >> 32) };
}

// ── PhiloxRng ─────────────────────────────────────────────────────────────────
// Stateful facade over the stateless core, same surface as SquaresRng. The 64-bit
// stream position is packed into ctr[0..1]; ctr[2..3] carry a fixed nonce (0) so
// the full 128-bit counter space is available if ever needed.
struct PhiloxRng {
    // ── Standard C++ UniformRandomBitGenerator interface ───────────────────────
    // Satisfies the named requirement, so PhiloxRng can be passed to any
    // <random> distribution (std::uniform_int_distribution, std::shuffle, etc.):
    //   std::uniform_int_distribution<int> d(0,99);  PhiloxRng g(seed);  d(g);
    using result_type = uint32_t;
    static constexpr result_type min() { return 0u; }
    static constexpr result_type max() { return 0xFFFFFFFFu; }
    result_type operator()() { return next(); }

    std::array<uint32_t,2> key = philoxMakeKey(0xC0FFEEull);
    uint64_t counter = 0;                 // sequential position
    std::array<uint32_t,4> buf{};         // cached block of 4 outputs
    int idx = 4;                          // 4 ⇒ buffer exhausted, refill on next()

    PhiloxRng() = default;
    explicit PhiloxRng(uint64_t seed) { seed64(seed); }

    void seed64(uint64_t seed) { key = philoxMakeKey(seed); counter = 0; idx = 4; }
    void seed(uint64_t s1, uint64_t s2) {
        key = philoxMakeKey(s1 ^ (0x9E3779B97F4A7C15ULL * (s2 | 1ULL)));
        counter = 0; idx = 4;
    }
    void setKey(std::array<uint32_t,2> k) { key = k; }
    std::array<uint32_t,2> getKey() const { return key; }
    void     setCounter(uint64_t c) { counter = c; idx = 4; }
    uint64_t getCounter() const     { return counter; }
    void     rewind() { counter = 0; idx = 4; }

    // ── Addressable (stateless) — the reversible core ──────────────────────────
    // The 4-word block at absolute block-index `blockPos`. Pure fn of (pos,key).
    std::array<uint32_t,4> block(uint64_t blockPos) const {
        std::array<uint32_t,4> ctr{
            (uint32_t)(blockPos & 0xFFFFFFFFu), (uint32_t)(blockPos >> 32), 0u, 0u };
        return philox4x32_10(ctr, key);
    }
    // Single 32-bit value at absolute stream position `pos` (4 per block).
    uint32_t at(uint64_t pos) const { return block(pos >> 2)[pos & 3u]; }
    float    atUniform(uint64_t pos) const { return philoxUniformFloat(at(pos)); }

    // ── Sequential facade ──────────────────────────────────────────────────────
    uint32_t next() {
        if (idx == 4) { buf = block(counter >> 2);
                        // align idx to counter's position within its block
                        idx = (int)(counter & 3u); }
        uint32_t v = buf[idx++];
        ++counter;
        if (idx == 4) { /* exhausted; next call refills */ }
        return v;
    }
    float  nextFloat()  { return philoxUniformFloat(next()); }
    double nextDouble() { uint32_t a = next(), b = next(); return philoxUniformDouble(a,b); }

    // ── Block drawing (PatternEngine use-case) — addressable, counter untouched ─
    void fillBlock(uint64_t base, float* out, int n) const {
        for (int i = 0; i < n; ++i) out[i] = atUniform(base + (uint64_t)i);
    }
    void drawBlock(float* out, int n) {
        fillBlock(counter, out, n);
        counter += (uint64_t)n; idx = 4;
    }
};

} // namespace redDot
