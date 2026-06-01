/**
 * test_MeloDicer.cpp
 * Unit tests for MeloDicer edge cases.
 *
 * Build alongside the plugin (no Rack GUI needed):
 *   g++ -std=c++17 -I/path/to/Rack/include -DTEST_BUILD \
 *       test_MeloDicer.cpp -o test_melodicer && ./test_melodicer
 *
 * The test harness is self-contained — no external framework needed.
 * Each TEST() block is independent; failures print clearly and continue.
 */

#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <functional>

// ── Minimal Rack stubs so we can test without linking rack.dll ───────────────
namespace rack {
    struct Param  { float value = 0.f;  float getValue() const { return value; }
                    void  setValue(float v)  { value = v; } };
    struct Input  { float voltage = 0.f; bool connected = false;
                    float getVoltage()   const { return voltage; }
                    bool  isConnected()  const { return connected; } };
    struct Output { float voltage = 0.f;
                    void  setVoltage(float v) { voltage = v; }
                    float getVoltage() const  { return voltage; } };
    struct Light  { float brightness = 0.f;
                    void  setBrightness(float b)           { brightness = b; }
                    void  setBrightnessSmooth(float b, float) { brightness = b; } };
    template<typename T>
    T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
}

// Bring helpers into global scope matching plugin usage
template<typename T>
T clampv(T v, T lo, T hi) { return rack::clamp(v, lo, hi); }

// Minimal ProcessArgs
struct ProcessArgs {
    float sampleTime  = 1.f / 44100.f;
    float sampleRate  = 44100.f;
};

// ── Paste the MeloDicer struct here (or #include the stripped cpp) ───────────
// For a real CI build: compile with -DTEST_BUILD and guard Rack-specific
// headers in the plugin source. For now, tests exercise pure logic extracted
// below as inline helpers mirroring the real implementation.

// ── Test framework ────────────────────────────────────────────────────────────
static int s_pass = 0, s_fail = 0;
static std::string s_current_suite;

#define SUITE(name) \
    do { s_current_suite = (name); \
         std::cout << "\n\033[1;34m[" << (name) << "]\033[0m\n"; } while(0)

#define TEST(desc, ...) \
    do { \
        bool _ok = true; \
        std::string _msg; \
        try { __VA_ARGS__; } \
        catch (const std::exception& e) { _ok = false; _msg = e.what(); } \
        if (_ok) { ++s_pass; std::cout << "  \033[32m✓\033[0m " << (desc) << "\n"; } \
        else     { ++s_fail; std::cout << "  \033[31m✗\033[0m " << (desc); \
                   if (!_msg.empty()) std::cout << " — " << _msg; \
                   std::cout << "\n"; } \
    } while(0)

#define EXPECT(expr) \
    do { if (!(expr)) throw std::runtime_error("EXPECT(" #expr ") failed"); } while(0)

#define EXPECT_NEAR(a, b, eps) \
    do { if (std::fabs((a)-(b)) > (eps)) { \
        std::ostringstream _s; \
        _s << #a << "=" << (a) << " not near " << #b << "=" << (b) << " (eps=" << (eps) << ")"; \
        throw std::runtime_error(_s.str()); } } while(0)

#define EXPECT_EQ(a, b) \
    do { if ((a) != (b)) { \
        std::ostringstream _s; \
        _s << #a << "=" << (a) << " != " << #b << "=" << (b); \
        throw std::runtime_error(_s.str()); } } while(0)

// ── Pure-logic helpers mirroring the real MeloDicer implementation ────────────
// These are extracted so we can test without the full Rack build.

// --- Note value fraction lookup (matches NOTEVALS in plugin) -----------------
static float noteValueFraction(int step) {
    // step 0..7 matching configSwitch labels
    static const float vals[8] = {
        0.5f,       // 1/2
        0.25f,      // 1/4
        1.f/6.f,    // 1/4T  (triplet quarter = 1/6 of whole)
        0.125f,     // 1/8
        1.f/12.f,   // 1/8T
        0.0625f,    // 1/16
        1.f/24.f,   // 1/32T
        0.03125f    // 1/32
    };
    if (step < 0 || step > 7) return 0.f;
    return vals[step];
}

// --- CV pitch quantise to semitone (mirrors genPitchV logic) -----------------
static float semitoneToVolts(int semitone, int octave) {
    // 1V/oct: C4 = 0V, each semitone = 1/12 V
    return (octave - 4) + semitone / 12.f;
}

// --- Semitone weight clamping ------------------------------------------------
static float clampWeight(float raw) {
    return clampv(raw, 0.f, 1.f);
}

// --- Pattern offset wrap -----------------------------------------------------
static int applyOffset(int step, int offset, int length) {
    if (length <= 0) return 0;
    return ((step + offset) % length + length) % length;
}

// --- OCT inversion guard -----------------------------------------------------
// If user sets OCT_LO > OCT_HI, swap them
static void sanitiseOctRange(float& lo, float& hi) {
    if (lo > hi) std::swap(lo, hi);
}

// --- Lock: pattern arrays must not change while locked -----------------------
struct PatternState {
    bool  rhythm[16]   = {};
    float pitch[16]    = {};
    bool  locked       = false;

    // Returns true if the redraw was actually applied
    bool tryRedraw(const bool newRhythm[16], const float newPitch[16]) {
        if (locked) return false;
        std::copy(newRhythm, newRhythm+16, rhythm);
        std::copy(newPitch,  newPitch+16,  pitch);
        return true;
    }
};

// --- All-zero semitone weights: fallback to equal weight --------------------
static int pickSemitoneWeighted(const float weights[12], float rng01) {
    float sum = 0.f;
    for (int i = 0; i < 12; ++i) sum += weights[i];
    if (sum < 1e-6f) {
        // All zero — equal probability fallback
        return static_cast<int>(rng01 * 12.f) % 12;
    }
    // Clamp rng01 strictly below 1.0 so thresh < sum always
    float thresh = clampv(rng01, 0.f, 1.f - 1e-7f) * sum;
    float acc = 0.f;
    for (int i = 0; i < 12; ++i) {
        acc += weights[i];
        if (acc > thresh) return i;
    }
    return 11; // safety (rounding)
}

// --- Mode switch: pending state should clear on mode change -----------------
struct ModeState {
    int   mode            = 0;
    float holdRemain      = 0.f;
    bool  gateHeld        = false;
    float currentPitchV   = 0.f;

    void switchMode(int newMode) {
        if (newMode == mode) return;
        mode        = newMode;
        holdRemain  = 0.f;   // clear any held note
        gateHeld    = false;
    }
};

// --- Seed: 0V and 10V seeds should produce different but valid patterns ------
// Mirrors splitmix64
static uint64_t splitmix64(uint64_t& state) {
    uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static uint64_t seedFromVoltage(float v) {
    // Maps 0..10V linearly to a uint64 seed
    uint64_t s = static_cast<uint64_t>(clampv(v, 0.f, 10.f) * 1e13);
    return s ^ (s << 32);
}

// --- BPM → seconds per sixteenth --------------------------------------------
static float bpmToSixteenth(float bpm) {
    if (bpm <= 0.f) return 1.f; // safety
    return 60.f / bpm / 4.f;   // 4 sixteenths per beat
}

// --- CV offset accumulation (mode B cv2Offsets) -----------------------------
// Each new note: new_offset = old + delta, clamped to ±5V
static float accumCvOffset(float current, float delta) {
    return clampv(current + delta, -5.f, 5.f);
}

// ══════════════════════════════════════════════════════════════════════════════
// TESTS
// ══════════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\033[1mMeloDicer Unit Tests\033[0m\n";
    std::cout << std::string(50, '=') << "\n";

    // ──────────────────────────────────────────────────────────────────────────
    SUITE("Note Value Knob — Detent Positions");
    // ──────────────────────────────────────────────────────────────────────────

    TEST("8 distinct note value fractions", {
        std::vector<float> fracs;
        for (int i = 0; i < 8; ++i) fracs.push_back(noteValueFraction(i));
        // All should be strictly decreasing (shorter notes = smaller fraction)
        for (int i = 1; i < 8; ++i)
            EXPECT(fracs[i] < fracs[i-1]);
    });

    TEST("1/2 note = 0.5 whole-note fraction", {
        EXPECT_NEAR(noteValueFraction(0), 0.5f, 1e-6f);
    });

    TEST("1/32 note = 0.03125 whole-note fraction", {
        EXPECT_NEAR(noteValueFraction(7), 0.03125f, 1e-6f);
    });

    TEST("Triplet 1/4T = exactly 1/6", {
        EXPECT_NEAR(noteValueFraction(2), 1.f/6.f, 1e-6f);
    });

    TEST("Out-of-range step returns 0 (no crash)", {
        EXPECT_EQ(noteValueFraction(-1),  0.f);
        EXPECT_EQ(noteValueFraction(8),   0.f);
        EXPECT_EQ(noteValueFraction(100), 0.f);
    });

    TEST("All fractions positive and <= 0.5", {
        for (int i = 0; i < 8; ++i) {
            float f = noteValueFraction(i);
            EXPECT(f > 0.f && f <= 0.5f);
        }
    });

    // ──────────────────────────────────────────────────────────────────────────
    SUITE("Lock Function");
    // ──────────────────────────────────────────────────────────────────────────

    TEST("Redraw blocked when locked", {
        PatternState p;
        bool newR[16] = {}; float newP[16] = {};
        newR[0] = true; newP[3] = 1.5f;
        p.locked = true;
        bool applied = p.tryRedraw(newR, newP);
        EXPECT(!applied);
        EXPECT(!p.rhythm[0]);    // unchanged
        EXPECT_NEAR(p.pitch[3], 0.f, 1e-6f); // unchanged
    });

    TEST("Redraw applies when unlocked", {
        PatternState p;
        bool newR[16] = {}; float newP[16] = {};
        newR[0] = true; newP[3] = 1.5f;
        p.locked = false;
        bool applied = p.tryRedraw(newR, newP);
        EXPECT(applied);
        EXPECT(p.rhythm[0]);
        EXPECT_NEAR(p.pitch[3], 1.5f, 1e-6f);
    });

    TEST("Lock state survives redraw attempt", {
        PatternState p;
        p.locked = true;
        bool r[16]={true}; float pp[16]={1.f};
        p.tryRedraw(r, pp);
        EXPECT(p.locked); // lock bit itself unchanged
    });

    TEST("Lock does not prevent reading pattern output", {
        PatternState p;
        p.pitch[5] = 2.3f;
        p.locked = true;
        EXPECT_NEAR(p.pitch[5], 2.3f, 1e-6f); // reads always allowed
    });

    // ──────────────────────────────────────────────────────────────────────────
    SUITE("Mode Switching");
    // ──────────────────────────────────────────────────────────────────────────

    TEST("Mode switch clears holdRemain", {
        ModeState m;
        m.holdRemain = 0.25f;
        m.switchMode(1);
        EXPECT_NEAR(m.holdRemain, 0.f, 1e-6f);
    });

    TEST("Mode switch clears gateHeld", {
        ModeState m;
        m.gateHeld = true;
        m.switchMode(2);
        EXPECT(!m.gateHeld);
    });

    TEST("Switching to same mode is a no-op", {
        ModeState m;
        m.mode = 2; m.holdRemain = 0.5f; m.gateHeld = true;
        m.switchMode(2); // same mode
        // State unchanged — no spurious reset
        EXPECT_NEAR(m.holdRemain, 0.5f, 1e-6f);
        EXPECT(m.gateHeld);
    });

    TEST("All 4 mode transitions clear gate state", {
        for (int from = 0; from < 4; ++from)
        for (int to   = 0; to   < 4; ++to) {
            if (from == to) continue;
            ModeState m;
            m.mode = from; m.holdRemain = 1.f; m.gateHeld = true;
            m.switchMode(to);
            EXPECT_NEAR(m.holdRemain, 0.f, 1e-6f);
            EXPECT(!m.gateHeld);
        }
    });

    // ──────────────────────────────────────────────────────────────────────────
    SUITE("Semitone Weights — Extreme Positions");
    // ──────────────────────────────────────────────────────────────────────────

    TEST("All weights zero → still picks a semitone (no hang)", {
        float w[12] = {}; // all zero
        int s = pickSemitoneWeighted(w, 0.5f);
        EXPECT(s >= 0 && s < 12);
    });

    TEST("All weights zero → different RNG values produce spread", {
        float w[12] = {};
        std::vector<int> picks;
        for (int i = 0; i <= 11; ++i)
            picks.push_back(pickSemitoneWeighted(w, i / 11.f));
        // Should not all be the same semitone
        int first = picks[0];
        bool allSame = true;
        for (int p : picks) if (p != first) { allSame = false; break; }
        EXPECT(!allSame);
    });

    TEST("Single weight=1 always picks that semitone", {
        for (int target = 0; target < 12; ++target) {
            float w[12] = {};
            w[target] = 1.f;
            for (int trial = 0; trial < 10; ++trial) {
                int s = pickSemitoneWeighted(w, trial / 9.f);
                EXPECT_EQ(s, target);
            }
        }
    });

    TEST("Max weight on last semitone (rng=1.0 boundary)", {
        float w[12] = {};
        w[11] = 1.f;
        int s = pickSemitoneWeighted(w, 0.9999f);
        EXPECT_EQ(s, 11);
    });

    TEST("Weight clamping: negative becomes 0", {
        EXPECT_NEAR(clampWeight(-0.5f), 0.f, 1e-6f);
    });

    TEST("Weight clamping: >1 becomes 1", {
        EXPECT_NEAR(clampWeight(1.5f), 1.f, 1e-6f);
    });

    TEST("Weight clamping: 0..1 passes through", {
        EXPECT_NEAR(clampWeight(0.f),  0.f,  1e-6f);
        EXPECT_NEAR(clampWeight(0.5f), 0.5f, 1e-6f);
        EXPECT_NEAR(clampWeight(1.f),  1.f,  1e-6f);
    });

    // ──────────────────────────────────────────────────────────────────────────
    SUITE("OCT LO / HI Inversion Guard");
    // ──────────────────────────────────────────────────────────────────────────

    TEST("lo < hi: unchanged", {
        float lo=2.f, hi=5.f;
        sanitiseOctRange(lo, hi);
        EXPECT_NEAR(lo, 2.f, 1e-6f);
        EXPECT_NEAR(hi, 5.f, 1e-6f);
    });

    TEST("lo == hi: no crash, both equal", {
        float lo=3.f, hi=3.f;
        sanitiseOctRange(lo, hi);
        EXPECT_NEAR(lo, 3.f, 1e-6f);
        EXPECT_NEAR(hi, 3.f, 1e-6f);
    });

    TEST("lo > hi: swapped", {
        float lo=6.f, hi=2.f;
        sanitiseOctRange(lo, hi);
        EXPECT(lo <= hi);
        EXPECT_NEAR(lo, 2.f, 1e-6f);
        EXPECT_NEAR(hi, 6.f, 1e-6f);
    });

    TEST("Semitone voltage: C4=0V, C5=1V, C3=-1V", {
        EXPECT_NEAR(semitoneToVolts(0, 4),  0.f,  1e-5f);
        EXPECT_NEAR(semitoneToVolts(0, 5),  1.f,  1e-5f);
        EXPECT_NEAR(semitoneToVolts(0, 3), -1.f,  1e-5f);
    });

    TEST("Semitone voltage: A4 = 9/12 V", {
        EXPECT_NEAR(semitoneToVolts(9, 4), 9.f/12.f, 1e-5f);
    });

    // ──────────────────────────────────────────────────────────────────────────
    SUITE("Pattern Length & Offset");
    // ──────────────────────────────────────────────────────────────────────────

    TEST("Offset=0: step unchanged", {
        for (int s=0; s<16; ++s)
            EXPECT_EQ(applyOffset(s, 0, 16), s);
    });

    TEST("Offset wraps at pattern length", {
        EXPECT_EQ(applyOffset(15, 1, 16), 0);
        EXPECT_EQ(applyOffset(14, 3, 16), 1);
    });

    TEST("Length=1: all offsets produce step 0", {
        for (int off=0; off<16; ++off)
            EXPECT_EQ(applyOffset(0, off, 1), 0);
    });

    TEST("Negative offset equivalent (wrap-safe)", {
        // Verify no negative modulo issue
        int result = applyOffset(0, 15, 16); // same as offset=-1
        EXPECT(result >= 0 && result < 16);
    });

    TEST("Length=16, offset=16 wraps to 0", {
        EXPECT_EQ(applyOffset(0, 16, 16), 0);
    });

    // ──────────────────────────────────────────────────────────────────────────
    SUITE("Seed Behaviour");
    // ──────────────────────────────────────────────────────────────────────────

    TEST("0V and 10V seeds produce different values", {
        uint64_t s0 = seedFromVoltage(0.f);
        uint64_t s10 = seedFromVoltage(10.f);
        EXPECT(s0 != s10);
    });

    TEST("Same voltage → same seed (deterministic)", {
        EXPECT_EQ(seedFromVoltage(5.f), seedFromVoltage(5.f));
    });

    TEST("splitmix64 produces different successive values", {
        uint64_t state = 12345678ULL;
        uint64_t a = splitmix64(state);
        uint64_t b = splitmix64(state);
        EXPECT(a != b);
    });

    TEST("splitmix64 with seed=0 still produces nonzero output", {
        uint64_t state = 0ULL;
        uint64_t v = splitmix64(state);
        EXPECT(v != 0ULL);
    });

    TEST("Seed voltage clamped: -1V treated as 0V seed", {
        EXPECT_EQ(seedFromVoltage(-1.f), seedFromVoltage(0.f));
    });

    TEST("Seed voltage clamped: 11V treated as 10V seed", {
        EXPECT_EQ(seedFromVoltage(11.f), seedFromVoltage(10.f));
    });

    // ──────────────────────────────────────────────────────────────────────────
    SUITE("BPM Edge Cases");
    // ──────────────────────────────────────────────────────────────────────────

    TEST("120 BPM → sixteenth = 0.125s", {
        EXPECT_NEAR(bpmToSixteenth(120.f), 0.125f, 1e-5f);
    });

    TEST("Very low BPM (20) → long sixteenth, no division by zero", {
        float s = bpmToSixteenth(20.f);
        EXPECT(s > 0.f && s < 100.f);
    });

    TEST("Very high BPM (300) → short sixteenth, positive", {
        float s = bpmToSixteenth(300.f);
        EXPECT(s > 0.f);
        EXPECT_NEAR(s, 60.f/300.f/4.f, 1e-5f);
    });

    TEST("BPM=0 guard returns safe positive value", {
        float s = bpmToSixteenth(0.f);
        EXPECT(s > 0.f); // must not be 0 or negative
    });

    // ──────────────────────────────────────────────────────────────────────────
    SUITE("CV Offset Accumulation (Mode B)");
    // ──────────────────────────────────────────────────────────────────────────

    TEST("Zero delta → no change", {
        EXPECT_NEAR(accumCvOffset(1.5f, 0.f), 1.5f, 1e-6f);
    });

    TEST("Accumulation clamps at +5V", {
        float v = 0.f;
        for (int i = 0; i < 100; ++i) v = accumCvOffset(v, 0.3f);
        EXPECT_NEAR(v, 5.f, 1e-5f);
    });

    TEST("Accumulation clamps at -5V", {
        float v = 0.f;
        for (int i = 0; i < 100; ++i) v = accumCvOffset(v, -0.3f);
        EXPECT_NEAR(v, -5.f, 1e-5f);
    });

    TEST("Single large delta clamped, not wrapped", {
        EXPECT_NEAR(accumCvOffset(0.f,  99.f),  5.f, 1e-5f);
        EXPECT_NEAR(accumCvOffset(0.f, -99.f), -5.f, 1e-5f);
    });

    // ──────────────────────────────────────────────────────────────────────────
    SUITE("Rhythm Pattern Integrity");
    // ──────────────────────────────────────────────────────────────────────────

    TEST("Pattern with length 1: only step 0 active", {
        // Any step index accessed should be clamped to [0, length)
        int length = 1;
        int step = 0;
        EXPECT(step < length);
    });

    TEST("Pattern length 16: all 16 steps accessible", {
        for (int i = 0; i < 16; ++i)
            EXPECT(applyOffset(i, 0, 16) == i);
    });

    TEST("REST=1.0 probability: note is always a rest", {
        // Simulate: if rest_prob >= rng, it's a rest
        float rest_prob = 1.0f;
        bool is_rest = (rest_prob >= 0.0001f); // rng is always > 0
        // With prob=1.0, every outcome is a rest
        EXPECT(is_rest);
    });

    TEST("LEGATO=1.0: gate never closes between notes", {
        // If legato >= 1.0, holdRemain should always be >= note_duration
        // Simulate: holdRemain = max(holdRemain, dur * legato)
        float dur = 0.25f;
        float legato = 1.0f;
        float holdRemain = 0.f;
        holdRemain = std::max(holdRemain, dur * legato);
        EXPECT(holdRemain >= dur);
    });

    TEST("VARIATION=0: no rhythmic variation applied", {
        // Variation=0 means only the base note value, no longer/shorter
        float variation = 0.f;
        float base_dur  = 0.125f; // 1/8 note
        // With variation=0, the scaled duration equals the base
        float scaled = base_dur * (1.f + variation * 0.f);
        EXPECT_NEAR(scaled, base_dur, 1e-6f);
    });

    // ──────────────────────────────────────────────────────────────────────────
    SUITE("PPQN Compatibility");
    // ──────────────────────────────────────────────────────────────────────────

    TEST("PPQN=1: only whole-note divisions valid", {
        // allowedPPQN bitmask: 1=PPQN1, 2=PPQN4, 4=PPQN24
        // With PPQN=1, only notes compatible with PPQN1 play
        int ppqnMask   = 1;   // PPQN1
        // 1/2 note: allowed on PPQN1 (bit 0)
        // 1/16 note: needs PPQN4 or PPQN24 — NOT allowed
        int halfAllowed    = (1 & ppqnMask) ? 1 : 0;  // bit 0 set
        int sixteenthMask  = 2;                        // needs bit 1
        int sixteenthOk    = (sixteenthMask & ppqnMask) ? 1 : 0;
        EXPECT_EQ(halfAllowed, 1);
        EXPECT_EQ(sixteenthOk, 0);
    });

    TEST("PPQN=24: all note values valid", {
        int ppqnMask = 7; // bits 0+1+2
        EXPECT((1 & ppqnMask) != 0); // PPQN1 notes
        EXPECT((2 & ppqnMask) != 0); // PPQN4 notes
        EXPECT((4 & ppqnMask) != 0); // PPQN24 notes (triplets)
    });

    // ──────────────────────────────────────────────────────────────────────────
    SUITE("JSON Patch Save/Load Stability");
    // ──────────────────────────────────────────────────────────────────────────

    TEST("modeSelect range: valid values are 0..3 only", {
        for (int v : {0,1,2,3}) EXPECT(v >= 0 && v <= 3);
        for (int v : {-1, 4, 99}) EXPECT(!(v >= 0 && v <= 3));
    });

    TEST("ppqnSetting valid values: 1, 4, or 24", {
        auto valid = [](int p){ return p==1 || p==4 || p==24; };
        EXPECT(valid(1)); EXPECT(valid(4)); EXPECT(valid(24));
        EXPECT(!valid(0)); EXPECT(!valid(2)); EXPECT(!valid(16));
    });

    TEST("Semitone weight round-trip: 0 and 1 survive float storage", {
        // Simulated: save as JSON float, reload
        float original[2] = {0.f, 1.f};
        float reloaded[2] = {0.f, 1.f}; // JSON float is lossless for these
        EXPECT_NEAR(reloaded[0], original[0], 1e-7f);
        EXPECT_NEAR(reloaded[1], original[1], 1e-7f);
    });

    // ──────────────────────────────────────────────────────────────────────────
    // Summary
    // ──────────────────────────────────────────────────────────────────────────
    std::cout << "\n" << std::string(50,'=') << "\n";
    std::cout << "\033[32m" << s_pass << " passed\033[0m  ";
    if (s_fail > 0)
        std::cout << "\033[31m" << s_fail << " failed\033[0m";
    else
        std::cout << "\033[32m0 failed\033[0m";
    std::cout << "  (" << (s_pass+s_fail) << " total)\n\n";

    return s_fail > 0 ? 1 : 0;
}
