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

// Step B: SIMD Philox — vectorize 4 consecutive block computations.
// Bit-identical to the scalar path (same algorithm, computed in parallel lanes).
// Uses simd_compat.hpp: Rack-bundled SIMDe — native SSE on x86, NEON on arm64.
#include "../third_party/simd/simd_compat.hpp"

namespace redDot {

#if defined(SIMD_HAVE_SSE)
// Helper: 4-lane unsigned 32x32->64 multiply, returning both hi and lo.
// _mm_mullo_epi32 (SSE4.1) gives the low 32 bits of all 4 lanes.
// For the high 32 bits, _mm_mul_epu32 (SSE2) processes 2 lanes at a time
// (the low 32 bits of each 64-bit lane), so we call it twice with shuffled inputs.
inline void mul4x32(__m128i a, __m128i b, __m128i& hi, __m128i& lo) {
    lo = _mm_mullo_epi32(a, b);  // low 32 bits, all 4 lanes (SSE4.1)
    __m128i prod02 = _mm_mul_epu32(a, b);  // {a[0]*b[0]_64, _, a[2]*b[2]_64, _}
    __m128i a13 = _mm_shuffle_epi32(a, _MM_SHUFFLE(2, 3, 0, 1));
    __m128i b13 = _mm_shuffle_epi32(b, _MM_SHUFFLE(2, 3, 0, 1));
    __m128i prod13 = _mm_mul_epu32(a13, b13);  // {a[1]*b[1]_64, _, a[3]*b[3]_64, _}
    __m128i hi02 = _mm_srli_epi64(prod02, 32);  // {hi[0], 0, hi[2], 0}
    __m128i hi13 = _mm_srli_epi64(prod13, 32);  // {hi[1], 0, hi[3], 0}
    hi = _mm_blend_epi16(hi02, _mm_slli_si128(hi13, 4), 0xCC);  // {hi[0], hi[1], hi[2], hi[3]}
}
#endif

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

// ── Seed-float → per-stream key derivation (shared by rhythm/melody/Change Alley) ──
// One 0..10 seed value produces N independent stream keys via a per-stream offset.
// This fixes the identical-derivation bug (PHILOX_KEY_DERIVATION_AND_CA_SEED.md
// Finding 1): rhythm and melody previously derived the SAME key from the same float,
// collapsing to one stream whenever the SEED input was used. Stream separation follows
// the documented S, S+1, S+2 model (additive) — Philox decorrelates adjacent keys by
// design (counter-based cipher), so the +stream offset yields independent sequences.
namespace seed {
    constexpr uint64_t STREAM_RHYTHM = 0, STREAM_MELODY = 1, STREAM_CA = 2;
    // STREAM_SOURCE_SELECT (q-mix): the per-voice generated-vs-external source pick is its
    // OWN Philox stream, S+3 in the additive model — decorrelated from rhythm/melody/CA so
    // "which notes" (q-mix) varies independently of "where they interleave" (melody). The
    // q-mix lane is wired in the geometry branch (editor lane 2 — see dsp/LaneMapping.hpp);
    // reseed is its OWN action and it shares the single SEED jack (consistent with CA).
    constexpr uint64_t STREAM_SOURCE_SELECT = 3;
    // OU-bridge interpolation noise: ONE per A/B pair, position-index keyed (no frac).
    // SEPARATE keys per stream (S+4/5/6) — distinct from base draws AND each other, so
    // the scrub noise is independent of the draws and decorrelated across streams.
    constexpr uint64_t STREAM_BRIDGE_R = 4, STREAM_BRIDGE_M = 5, STREAM_BRIDGE_Q = 6;
    // seedFloat 0..10 → 64-bit key for the given stream. Same float + different stream
    // → different key. seed64() conditions the key again via philoxMakeKey, so a small
    // additive offset still lands on a well-separated stream.
    inline uint64_t deriveKey(float seedFloat, uint64_t stream) {
        float s = seedFloat < 0.f ? 0.f : (seedFloat > 10.f ? 10.f : seedFloat);
        uint64_t sd = (uint64_t)((double)s / 10.0 * (double)0xFFFFFFFFFFFFFFFFULL);
        return sd + stream;   // documented S, S+1, S+2 model
    }
}

// ── PhiloxRng ─────────────────────────────────────────────────────────────────
// Stateful facade over the stateless core, same surface as SquaresRng. The 64-bit
// stream position is packed into ctr[0..1]; ctr[2..3] carry a fixed nonce (0) so
// the full 128-bit counter space is available if ever needed. The nonce is UNUSED and stays so:
// per-voice is cursor-packed (rhythm/melody), CA separates by key, and the dice is PER-STREAM
// (rhythm/melody/CA/qmix each) never per-voice — so per-voice nonce addressing buys nothing. See
// docs/design/PHILOX_NONCE_ADDRESSING.md.
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
    // The 4-word block at absolute block-index blockPos. Pure fn of (pos,key).
    std::array<uint32_t,4> block(uint64_t blockPos) const {
        std::array<uint32_t,4> ctr{
            (uint32_t)(blockPos & 0xFFFFFFFFu), (uint32_t)(blockPos >> 32), 0u, 0u };
        return philox4x32_10(ctr, key);
    }
    // Single 32-bit value at absolute stream position pos (4 per block).
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
    // Step A: 4x batching — compute each Philox block ONCE, consume all 4 outputs.
    // The old fillBlock called atUniform(base+i) per element, which recomputed the
    // entire 10-round Philox block for each of the 4 outputs (75% waste). Now each
    // block is computed once and all 4 outputs are consumed. Bit-identical.
#if defined(SIMD_HAVE_SSE)
    // Step B: SIMD Philox — compute 4 consecutive blocks (16 outputs) in parallel
    // using SSE4.2. Bit-identical to 4× scalar block() calls. Each lane processes
    // one block; the 4 counter words are in separate SSE registers (ctr0..ctr3).
    void fillBlock4xSimd(uint64_t base, float* out) const {
        // Pack 4 consecutive block positions' counters
        uint64_t bp = base >> 2;
        __m128i ctr0 = _mm_setr_epi32(
            (int32_t)(uint32_t)(bp),       (int32_t)(uint32_t)(bp + 1),
            (int32_t)(uint32_t)(bp + 2),   (int32_t)(uint32_t)(bp + 3));
        __m128i ctr1 = _mm_setr_epi32(
            (int32_t)(uint32_t)(bp >> 32), (int32_t)(uint32_t)((bp + 1) >> 32),
            (int32_t)(uint32_t)((bp + 2) >> 32), (int32_t)(uint32_t)((bp + 3) >> 32));
        __m128i ctr2 = _mm_setzero_si128();
        __m128i ctr3 = _mm_setzero_si128();
        // Philox constants
        const __m128i M0 = _mm_set1_epi32((int32_t)0xD2511F53u);
        const __m128i M1 = _mm_set1_epi32((int32_t)0xCD9E8D57u);
        __m128i k0 = _mm_set1_epi32((int32_t)key[0]);
        __m128i k1 = _mm_set1_epi32((int32_t)key[1]);
        const __m128i W0 = _mm_set1_epi32((int32_t)0x9E3779B9u);
        const __m128i W1 = _mm_set1_epi32((int32_t)0xBB67AE85u);
        // 10 rounds (round 0 uses raw key; rounds 1-9 bump key before use)
        for (int r = 0; r < 10; ++r) {
            __m128i hi0, lo0, hi1, lo1;
            mul4x32(M0, ctr0, hi0, lo0);
            mul4x32(M1, ctr2, hi1, lo1);
            // out = {hi1 ^ ctr1 ^ k0, lo1, hi0 ^ ctr3 ^ k1, lo0}
            __m128i new0 = _mm_xor_si128(_mm_xor_si128(hi1, ctr1), k0);
            __m128i new1 = lo1;
            __m128i new2 = _mm_xor_si128(_mm_xor_si128(hi0, ctr3), k1);
            __m128i new3 = lo0;
            ctr0 = new0; ctr1 = new1; ctr2 = new2; ctr3 = new3;
            if (r < 9) { k0 = _mm_add_epi32(k0, W0); k1 = _mm_add_epi32(k1, W1); }
        }
        // Transpose 4x4: {ctr0..3}[lane] → block[lane] = {ctr0..3}[lane]
        __m128i t0 = _mm_unpacklo_epi32(ctr0, ctr1);
        __m128i t1 = _mm_unpackhi_epi32(ctr0, ctr1);
        __m128i t2 = _mm_unpacklo_epi32(ctr2, ctr3);
        __m128i t3 = _mm_unpackhi_epi32(ctr2, ctr3);
        __m128i blk0 = _mm_unpacklo_epi64(t0, t2);  // block 0's 4 outputs
        __m128i blk1 = _mm_unpackhi_epi64(t0, t2);  // block 1
        __m128i blk2 = _mm_unpacklo_epi64(t1, t3);  // block 2
        __m128i blk3 = _mm_unpackhi_epi64(t1, t3);  // block 3
        // Convert to floats: (bits >> 8) * (1/2^24)
        const __m128 scale = _mm_set1_ps(1.0f / 16777216.0f);
        _mm_store_ps(out,      _mm_mul_ps(_mm_cvtepi32_ps(_mm_srli_epi32(blk0, 8)), scale));
        _mm_store_ps(out + 4,  _mm_mul_ps(_mm_cvtepi32_ps(_mm_srli_epi32(blk1, 8)), scale));
        _mm_store_ps(out + 8,  _mm_mul_ps(_mm_cvtepi32_ps(_mm_srli_epi32(blk2, 8)), scale));
        _mm_store_ps(out + 12, _mm_mul_ps(_mm_cvtepi32_ps(_mm_srli_epi32(blk3, 8)), scale));
    }
#endif
    void fillBlock(uint64_t base, float* out, int n) const {
        int i = 0;
        uint64_t pos = base;
        // Leading partial block (base not block-aligned)
        int lead = (int)(base & 3u);
        if (lead > 0 && n > 0) {
            auto b = block(base >> 2);
            for (int j = lead; j < 4 && i < n; ++j)
                out[i++] = philoxUniformFloat(b[j]);
            pos = (base & ~3ull) + 4;
        }
#if defined(SIMD_HAVE_SSE)
        // Step B: SIMD — process 4 blocks (16 outputs) at a time
        while (i + 16 <= n) {
            fillBlock4xSimd(pos, out + i);
            i += 16; pos += 16;
        }
#endif
        // Remaining full blocks (scalar)
        while (i + 4 <= n) {
            auto b = block(pos >> 2);
            out[i]   = philoxUniformFloat(b[0]);
            out[i+1] = philoxUniformFloat(b[1]);
            out[i+2] = philoxUniformFloat(b[2]);
            out[i+3] = philoxUniformFloat(b[3]);
            i += 4; pos += 4;
        }
        // Trailing partial block
        if (i < n) {
            auto b = block(pos >> 2);
            for (int j = 0; i < n; ++j)
                out[i++] = philoxUniformFloat(b[j]);
        }
    }
    void drawBlock(float* out, int n) {
        fillBlock(counter, out, n);
        counter += (uint64_t)n; idx = 4;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Philox4x64-10 — 64-bit variant (the std::philox4x64 algorithm). 4 x 64-bit
// counter, 2 x 64-bit key, 10 rounds. Same lane convention as the 4x32 core;
// verified bit-exact against std::philox4x64's documented anchor (default-seeded
// 10000th invocation = 3409172418970261260).
inline std::array<uint64_t,4> philox4x64_10(std::array<uint64_t,4> ctr,
                                            std::array<uint64_t,2> key) {
    constexpr uint64_t M0 = 0xD2E7470EE14C6C93ull;
    constexpr uint64_t M1 = 0xCA5A826395121157ull;
    constexpr uint64_t W0 = 0x9E3779B97F4A7C15ull;   // golden ratio (64-bit)
    constexpr uint64_t W1 = 0xBB67AE8584CAA73Bull;   // sqrt(3)-1 (64-bit)

    auto round = [](std::array<uint64_t,4> c, std::array<uint64_t,2> k) {
        __uint128_t p0 = (__uint128_t)M0 * c[0];
        __uint128_t p1 = (__uint128_t)M1 * c[2];
        uint64_t hi0 = (uint64_t)(p0 >> 64), lo0 = (uint64_t)p0;
        uint64_t hi1 = (uint64_t)(p1 >> 64), lo1 = (uint64_t)p1;
        return std::array<uint64_t,4>{ hi1 ^ c[1] ^ k[0], lo1, hi0 ^ c[3] ^ k[1], lo0 };
    };
    ctr = round(ctr, key);
    for (int r = 1; r < 10; ++r) { key[0] += W0; key[1] += W1; ctr = round(ctr, key); }
    return ctr;
}

inline std::array<uint64_t,2> philox64MakeKey(uint64_t seed) {
    uint64_t z = seed + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z =  z ^ (z >> 31);
    uint64_t z2 = z + 0x9E3779B97F4A7C15ULL;
    z2 = (z2 ^ (z2 >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z2 =  z2 ^ (z2 >> 31);
    return { z, z2 };
}

// 64-bit counter-based engine, same interface family as PhiloxRng but with a
// uint64_t result_type — the analogue of std::philox4x64.
struct Philox4x64Rng {
    using result_type = uint64_t;
    static constexpr result_type min() { return 0ull; }
    static constexpr result_type max() { return ~0ull; }
    result_type operator()() { return next(); }

    std::array<uint64_t,2> key = philox64MakeKey(0xC0FFEEull);
    uint64_t counter = 0;
    std::array<uint64_t,4> buf{};
    int idx = 4;

    Philox4x64Rng() = default;
    explicit Philox4x64Rng(uint64_t seed) { seed64(seed); }

    void seed64(uint64_t seed) { key = philox64MakeKey(seed); counter = 0; idx = 4; }
    void seed(uint64_t s1, uint64_t s2) {
        key = philox64MakeKey(s1 ^ (0x9E3779B97F4A7C15ULL * (s2 | 1ULL)));
        counter = 0; idx = 4;
    }
    void setKey(std::array<uint64_t,2> k) { key = k; }
    std::array<uint64_t,2> getKey() const { return key; }
    void     setCounter(uint64_t c) { counter = c; idx = 4; }
    uint64_t getCounter() const     { return counter; }
    void     rewind() { counter = 0; idx = 4; }

    std::array<uint64_t,4> block(uint64_t blockPos) const {
        std::array<uint64_t,4> ctr{ blockPos, 0u, 0u, 0u };
        return philox4x64_10(ctr, key);
    }
    uint64_t at(uint64_t pos) const { return block(pos >> 2)[pos & 3u]; }
    double   atUniform(uint64_t pos) const {
        // 53-bit mantissa from the top bits of a 64-bit word.
        return (at(pos) >> 11) * (1.0 / 9007199254740992.0);   // 2^-53
    }

    uint64_t next() {
        if (idx == 4) { buf = block(counter >> 2); idx = (int)(counter & 3u); }
        uint64_t v = buf[idx++]; ++counter; return v;
    }
    double nextDouble() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
    float  nextFloat()  { return (float)nextDouble(); }

    void fillBlock(uint64_t base, double* out, int n) const {
        for (int i = 0; i < n; ++i) out[i] = atUniform(base + (uint64_t)i);
    }
    void drawBlock(double* out, int n) { fillBlock(counter, out, n); counter += (uint64_t)n; idx = 4; }
};

} // namespace redDot
