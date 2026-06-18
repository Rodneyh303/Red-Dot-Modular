#pragma once
/**
 * SquaresRng.hpp
 * Counter-based PRNG (Squares, by Bernard Widynski — a counter-based RNG built on
 * the Philox/middle-square idea). https://arxiv.org/abs/2004.06278
 *
 * WHY this exists (vs the current Xoroshiro128Plus):
 *   Squares is STATELESS and ADDRESSABLE. The random value for draw index N is a
 *   pure function squares64(N, key) — it does not depend on having generated
 *   indices 0..N-1 first. That makes the stream:
 *     • reversible / random-access: value(N) is the same whether reached going
 *       forward, backward, or by jumping — which is exactly what a phase-driven
 *       reverse/scrub mode needs (replay the same draws backwards).
 *     • cheap to "store": persist the 64-bit key, not an array of draws.
 *   A new key (e.g. on a dice-button press) gives a new sequence, so exact reverse
 *   holds for the LIFETIME OF ONE DRAW (one key), which is the intended scope.
 *
 * Drop-in intent: exposes seed()/next()/nextDouble()/nextFloat() so it can replace
 * the Xoroshiro usage in PatternEngine, PLUS at(counter) / fillBlock() for the
 * addressable block-drawing pattern (~800+ draws per regeneration, drawn in blocks
 * indexed by counter).
 *
 * Header-only, no Rack dependency, nanosvg-irrelevant (pure DSP util).
 */

#include <cstdint>
#include <cstring>

namespace redDot {

// ── The Squares algorithm (verbatim, Widynski) ──────────────────────────────
// ctr  : counter — the addressable index into the stream.
// key   : the stream identity (an odd, well-mixed constant; see makeKey()).
inline uint64_t squares64(uint64_t ctr, uint64_t key) {
    uint64_t t, x, y, z;
    y = x = ctr * key; z = y + key;
    x = x*x + y; x = (x>>32) | (x<<32); /* round 1 */
    x = x*x + z; x = (x>>32) | (x<<32); /* round 2 */
    x = x*x + y; x = (x>>32) | (x<<32); /* round 3 */
    t = x = x*x + z; x = (x>>32) | (x<<32); /* round 4 */
    return t ^ ((x*x + y) >> 32);        /* round 5 */
}

// ── Uniform [0,1) from 64 random bits (dyadic, branch-free) ──────────────────
inline double squaresUniformDouble(uint64_t random_bits) {
    constexpr uint64_t exponent_mask = 0x3FF0000000000000ULL; // 1.0 exponent
    constexpr uint64_t mantissa_mask = 0x000FFFFFFFFFFFFFULL; // 52-bit mantissa
    uint64_t mapped = exponent_mask | (random_bits & mantissa_mask); // → [1.0, 2.0)
    double result;
    std::memcpy(&result, &mapped, sizeof(double));
    return result - 1.0;                                            // → [0.0, 1.0)
}

// ── Key conditioning ─────────────────────────────────────────────────────────
// Squares wants a key with a good spread of bits and an odd low byte. Any 64-bit
// seed is run through a SplitMix64-style mix and forced odd so weak seeds (e.g. a
// small integer derived from a 0..10V CV) still produce a high-quality stream.
inline uint64_t squaresMakeKey(uint64_t seed) {
    uint64_t z = seed + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z =  z ^ (z >> 31);
    return z | 1ULL;   // ensure odd
}

// ── SquaresRng ────────────────────────────────────────────────────────────────
// A thin stateful facade over the stateless core. Holds a key + a running counter
// so it can act like a conventional sequential RNG (next()/nextFloat()), while
// also exposing the addressable at(counter) form for reversible/random-access use.
struct SquaresRng {
    uint64_t key     = squaresMakeKey(0xC0FFEEULL);
    uint64_t counter = 0;

    SquaresRng() = default;
    explicit SquaresRng(uint64_t seed) { seed64(seed); }

    // ── Seeding ───────────────────────────────────────────────────────────────
    // Set the stream identity (conditions the seed into a good key) and rewind.
    void seed64(uint64_t seed) { key = squaresMakeKey(seed); counter = 0; }

    // Two-word seed, to mirror Xoroshiro128Plus::seed(s1, s2) for drop-in. The two
    // words are folded into one conditioned key.
    void seed(uint64_t s1, uint64_t s2) {
        key = squaresMakeKey(s1 ^ (0x9E3779B97F4A7C15ULL * (s2 | 1ULL)));
        counter = 0;
    }

    void setKey(uint64_t rawConditionedKey) { key = rawConditionedKey | 1ULL; }
    uint64_t getKey() const { return key; }

    void   setCounter(uint64_t c) { counter = c; }
    uint64_t getCounter() const   { return counter; }
    void   rewind()  { counter = 0; }

    // ── Addressable (stateless) access — the reversible core ───────────────────
    // The raw 64-bit value at an absolute stream position. Pure function of (pos,
    // key); does not touch `counter`. value(N) == value(N) forever, in any order.
    uint64_t at(uint64_t pos) const { return squares64(pos, key); }

    // Uniform [0,1) double / float at an absolute position.
    double atUniform(uint64_t pos) const { return squaresUniformDouble(at(pos)); }
    float  atUniformF(uint64_t pos) const { return (float)atUniform(pos); }

    // ── Sequential access — conventional RNG facade ────────────────────────────
    uint64_t next()        { return squares64(counter++, key); }   // raw 64 bits
    double   nextDouble()  { return squaresUniformDouble(next()); } // [0,1)
    float    nextFloat()   { return (float)nextDouble(); }          // [0,1)

    // ── Block drawing (the PatternEngine use-case) ─────────────────────────────
    // Fill out[0..n) with uniform floats starting at absolute position `base`,
    // WITHOUT disturbing `counter`. Deterministic + addressable: re-filling the
    // same (base,n,key) reproduces the block exactly — forwards or backwards —
    // which is the property that lets phase mode replay draws in reverse.
    void fillBlock(uint64_t base, float* out, int n) const {
        for (int i = 0; i < n; ++i) out[i] = atUniformF(base + (uint64_t)i);
    }
    // Same, advancing `counter` past the block (sequential convenience).
    void drawBlock(float* out, int n) {
        fillBlock(counter, out, n);
        counter += (uint64_t)n;
    }
};

} // namespace redDot
