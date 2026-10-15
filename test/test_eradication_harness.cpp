// test_eradication_harness.cpp — Phase 0 verification harness (plans/ERADICATION_cc_brief.md).
// Three tests that make the voice bugs CATCHABLE without Rack:
//   1. All-voices-have-data: N active voices → EVERY voiceSlot 0..N-1 has nonzero lane data.
//   2. Voice-consistency: slewed*[v] → random_[v] promote uses the correct voice index
//      (no off-by-one: voice v's random_ contains voice v's data, not v±1's).
//   3. Bit-compare golden: fixed seed → hash of all voices' output → deterministic + reproducible.
#include "engines/PatternEngine.hpp"
#include "engines/SequencerEngine.hpp"
#include <cstdio>
#include <cstring>
#include <cstdint>

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) ++passed;
    else { ++failed; std::printf("  FAIL: %s\n", what); }
}

// ── Shared setup: fixed seed + fixed params → deterministic engine state ──
static PatternEngine makeSeededEngine(int nPolyVoices) {
    PatternEngine pe;
    pe.seedRhythmPhilox(3.14f);
    pe.seedMelodyPhilox(2.71f);
    pe.seedQmixPhilox(1.41f);
    pe.seedBridgeRhythmPhilox(3.14f);
    pe.seedBridgeMelodyPhilox(2.71f);
    pe.seedBridgeQmixPhilox(1.41f);
    pe.numPolyVoicesHint = nPolyVoices;
    pe.syncPolyVoiceCount();
    return pe;
}

// Full pipeline: seeds → redraw → recompute (what the engine does on a dice roll).
static void rollDice(PatternEngine& pe, const PatternInput& in) {
    pe.rhythmSeedPending = true;  pe.rhythmSeedPendingFloat = 3.14f;
    pe.melodySeedPending = true;  pe.melodySeedPendingFloat = 2.71f;
    pe.qmixSeedPending   = true;  pe.qmixSeedPendingFloat   = 1.41f;
    pe.applyPendingSeedsAndRedraw(in);
}

int main() {
    std::printf("Phase 0 eradication harness\n");

    // ════════════════════════════════════════════════════════════════════════
    // TEST 1: All-voices-have-data (catches Bug A — last voice shows zero)
    // For N in {2, 6, 15}: N active voices (voiceSlot 0..N-1), roll dice,
    // assert EVERY voiceSlot has nonzero data in each lane.
    // ════════════════════════════════════════════════════════════════════════
    std::printf("[Test 1: all-voices-have-data]\n");
    for (int N : {2, 6, 15}) {
        PatternEngine pe = makeSeededEngine(N - 1);  // N-1 poly + V1 = N total
        PatternInput in;
        rollDice(pe, in);

        char label[96];
        for (int v = 0; v < N; ++v) {  // voiceSlot 0..N-1 (V1..V(N))
            // Rhythm (REST lane)
            bool restNonzero = false;
            for (int s = 0; s < 16; ++s)
                if (pe.polyRandom(v, SequencerEngine::PL_REST)[s] != 0.f) { restNonzero = true; break; }
            std::snprintf(label, sizeof(label), "N=%d voiceSlot=%d (V%d) REST nonzero", N, v, v + 1);
            check(restNonzero, label);

            // Melody
            bool melNonzero = false;
            for (int s = 0; s < 16; ++s)
                if (pe.polyRandom(v, SequencerEngine::PL_MELODY)[s] != 0.f) { melNonzero = true; break; }
            std::snprintf(label, sizeof(label), "N=%d voiceSlot=%d (V%d) MELODY nonzero", N, v, v + 1);
            check(melNonzero, label);

            // Qmix
            bool qmixNonzero = false;
            for (int s = 0; s < 16; ++s)
                if (pe.polyRandom(v, SequencerEngine::PL_QMIX)[s] != 0.f) { qmixNonzero = true; break; }
            std::snprintf(label, sizeof(label), "N=%d voiceSlot=%d (V%d) QMIX nonzero", N, v, v + 1);
            check(qmixNonzero, label);
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // TEST 2: Voice-consistency (catches Bug B — two patterns flipping)
    // The slewed*→random_ promote must use the SAME voice index on both sides:
    // random_[v] must contain slewed*[v]'s data (not v±1's). After rollDice
    // (sandsActive=false, cachedSpreadInitialized=false → the !sandsActive
    // promote writes slewed*[v] directly into random_[v]), we verify
    // polyRandom(v, lane)[s] == slewed*[v][s] for EVERY voice. A ±1 index error
    // would put voice v±1's data into random_[v] — caught here.
    // ════════════════════════════════════════════════════════════════════════
    std::printf("[Test 2: voice-consistency (slewed*→random_ promote index)]\n");
    {
        PatternEngine pe = makeSeededEngine(15);
        // Ensure the promote runs WITHOUT spread (writes slewed* directly)
        pe.sandsActive = false;
        pe.cachedSpreadInitialized = false;
        PatternInput in;
        rollDice(pe, in);

        char label[96];
        int mismatches = 0;
        for (int v = 0; v < 16; ++v) {
            bool restOk = true, melOk = true, qmixOk = true;
            for (int s = 0; s < 16; ++s) {
                // The !sandsActive promote writes: polyRandom(v,PL_REST)[s] = slewedRhythm[v][s]
                // (no spread when cachedSpreadInitialized=false). Check bit-equality.
                float gotRest = pe.polyRandom(v, SequencerEngine::PL_REST)[s];
                float wantRest = pe.slewedRhythm[v][s];
                if (gotRest != wantRest) { restOk = false; ++mismatches; }
                float gotMel = pe.polyRandom(v, SequencerEngine::PL_MELODY)[s];
                float wantMel = pe.slewedMelody[v][s];
                if (gotMel != wantMel) { melOk = false; ++mismatches; }
                float gotQmix = pe.polyRandom(v, SequencerEngine::PL_QMIX)[s];
                float wantQmix = pe.slewedQmix[v][s];
                if (gotQmix != wantQmix) { qmixOk = false; ++mismatches; }
            }
            std::snprintf(label, sizeof(label), "voiceSlot=%d REST random_==slewed_[v] (promote index correct)", v);
            check(restOk, label);
            std::snprintf(label, sizeof(label), "voiceSlot=%d MELODY random_==slewed_[v] (promote index correct)", v);
            check(melOk, label);
            std::snprintf(label, sizeof(label), "voiceSlot=%d QMIX random_==slewed_[v] (promote index correct)", v);
            check(qmixOk, label);
        }
        std::printf("  (total value mismatches: %d)\n", mismatches);
    }

    // ════════════════════════════════════════════════════════════════════════
    // TEST 3: Bit-compare golden (fixed seed → deterministic hash)
    // Hash ALL voices' per-lane output (polyRandom for all 16 voiceSlots ×
    // 7 lanes × 16 steps). The hash must be IDENTICAL on every run (determinism)
    // and any eradication step that changes an already-correct voice's output
    // changes the hash → caught.
    // ════════════════════════════════════════════════════════════════════════
    std::printf("[Test 3: bit-compare golden hash]\n");
    {
        // Run the full pipeline twice and compare hashes (determinism check).
        uint64_t hash1 = 0, hash2 = 0;
        for (int run = 0; run < 2; ++run) {
            PatternEngine pe = makeSeededEngine(15);
            PatternInput in;
            rollDice(pe, in);

            uint64_t h = 1469598103934665603ULL;  // FNV-1a offset basis
            for (int v = 0; v < 16; ++v) {
                for (int lane = 0; lane < SequencerEngine::PL_LANES; ++lane) {
                    const float* arr = pe.polyRandom(v, lane);
                    for (int s = 0; s < 16; ++s) {
                        uint32_t bits;
                        std::memcpy(&bits, &arr[s], sizeof(bits));
                        h ^= bits;
                        h *= 1099511628211ULL;  // FNV-1a prime
                    }
                }
            }
            if (run == 0) hash1 = h; else hash2 = h;
        }
        check(hash1 == hash2, "golden hash deterministic (run1 == run2)");
        std::printf("  golden hash: 0x%016llx\n", (unsigned long long)hash1);

        // Also verify the hash changes when we use a different seed (sanity:
        // the hash is actually capturing the data, not always the same value).
        {
            PatternEngine pe;
            pe.seedRhythmPhilox(9.99f);  // different seed
            pe.seedMelodyPhilox(9.99f);
            pe.seedQmixPhilox(9.99f);
            pe.seedBridgeRhythmPhilox(9.99f);
            pe.seedBridgeMelodyPhilox(9.99f);
            pe.seedBridgeQmixPhilox(9.99f);
            pe.numPolyVoicesHint = 15;
            pe.syncPolyVoiceCount();
            PatternInput in;
            pe.rhythmSeedPending = true; pe.rhythmSeedPendingFloat = 9.99f;
            pe.melodySeedPending = true; pe.melodySeedPendingFloat = 9.99f;
            pe.qmixSeedPending   = true; pe.qmixSeedPendingFloat   = 9.99f;
            pe.applyPendingSeedsAndRedraw(in);

            uint64_t h = 1469598103934665603ULL;
            for (int v = 0; v < 16; ++v)
                for (int lane = 0; lane < SequencerEngine::PL_LANES; ++lane) {
                    const float* arr = pe.polyRandom(v, lane);
                    for (int s = 0; s < 16; ++s) {
                        uint32_t bits;
                        std::memcpy(&bits, &arr[s], sizeof(bits));
                        h ^= bits;
                        h *= 1099511628211ULL;
                    }
                }
            check(h != hash1, "golden hash different for different seed (captures data)");
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // TEST 4: V16 melody stability (STEP 1 of the V16-flicker protocol).
    // 16 active voices, fixed seed, roll dice. For voiceSlot 15 (V16):
    //   (i)   random_[15][PL_MELODY] NONZERO, and the BAR SOURCE
    //         pubSlewedPolyMelody[15] nonzero too
    //   (ii)  STABLE across refresh cycles: mix=0.30 → refresh → read A;
    //         mix=0.31 → refresh; mix=0.30 → refresh → read B. A == B
    //         (bit-identical). This is ALSO the ForVoice(15) vs
    //         full-recompute consistency law: A comes from the FULL
    //         recompute (first 0.30 = cache miss), B from ForVoice(15)
    //         (cache hit) — the two writers must agree at the same mix.
    //   (iii) V16's values must be DISTINCT from V15's (an off-by-one at
    //         slot 15 would make ForVoice(15) read V15's cache slots).
    //   (iv)  rhythm-mix modulation must NOT perturb V16's melody values
    //         (the reported repro: modulating rhythm-mix flickers the
    //         MELODY bars on V16 only).
    // Nonzero melodySlew keeps the ForVoice copula path live (r != 0).
    // ════════════════════════════════════════════════════════════════════════
    std::printf("[Test 4: V16 melody stability (flicker)]\n");
    {
        PatternEngine pe = makeSeededEngine(15);   // 15 poly + V1 = 16 voices
        pe.displayedVoice_ = 15;                    // V16 displayed (the flickering bars)
        PatternInput in;
        rollDice(pe, in);

        // (i) nonzero — raw store and bar source
        bool rawNonzero = false, pubNonzero = false;
        for (int s = 0; s < 16; ++s) {
            if (pe.polyRandom(15, SequencerEngine::PL_MELODY)[s] != 0.f) rawNonzero = true;
            if (pe.pubSlewedPolyMelody[15][s] != 0.f) pubNonzero = true;
        }
        check(rawNonzero, "V16 (voiceSlot 15) MELODY nonzero (random_)");
        check(pubNonzero, "V16 (voiceSlot 15) MELODY nonzero (pubSlewed_ — bar source)");

        // (ii) same-mix determinism: full recompute at 0.30, ForVoice at 0.31,
        // ForVoice back at 0.30 — the two 0.30 reads must be bit-identical.
        float rawA[16], rawB[16], pubA[16], pubB[16];
        pe.latchMix(0.f, 0.30f, 0.f, 0.f, 0.4f, 0.f, true, true, true);
        pe.refreshVisualCache(in);   // first 0.30: z-cache cold at this r → FULL recompute
        for (int s = 0; s < 16; ++s) {
            rawA[s] = pe.polyRandom(15, SequencerEngine::PL_MELODY)[s];
            pubA[s] = pe.pubSlewedPolyMelody[15][s];
        }
        pe.latchMix(0.f, 0.31f, 0.f, 0.f, 0.4f, 0.f, true, true, true);
        pe.refreshVisualCache(in);   // 0.31: ForVoice(15), cache hit
        pe.latchMix(0.f, 0.30f, 0.f, 0.f, 0.4f, 0.f, true, true, true);
        pe.refreshVisualCache(in);   // back to 0.30: ForVoice(15), cache hit
        bool rawStable = true, pubStable = true;
        for (int s = 0; s < 16; ++s) {
            rawB[s] = pe.polyRandom(15, SequencerEngine::PL_MELODY)[s];
            pubB[s] = pe.pubSlewedPolyMelody[15][s];
            if (rawB[s] != rawA[s]) rawStable = false;
            if (pubB[s] != pubA[s]) pubStable = false;
        }
        check(rawStable, "V16 MELODY stable across refresh cycles (random_, same mix)");
        check(pubStable, "V16 MELODY stable across refresh cycles (pubSlewed_, same mix) — ForVoice(15) == full recompute");

        // (iii) distinct from V15 (off-by-one detector: ForVoice(15) must read
        // V16's cache slots, not V15's)
        bool distinctFromV15 = false;
        for (int s = 0; s < 16; ++s)
            if (pubB[s] != pe.pubSlewedPolyMelody[14][s]) { distinctFromV15 = true; break; }
        check(distinctFromV15, "V16 MELODY distinct from V15 (ForVoice slot not off-by-one)");

        // (iv) rhythm-mix modulation must not perturb V16's melody values
        pe.latchMix(0.70f, 0.30f, 0.f, 0.f, 0.4f, 0.f, true, true, true);
        pe.refreshVisualCache(in);   // rhythm mix change → rhythm ForVoice path only
        bool isoRaw = true, isoPub = true;
        for (int s = 0; s < 16; ++s) {
            if (pe.polyRandom(15, SequencerEngine::PL_MELODY)[s] != rawB[s]) isoRaw = false;
            if (pe.pubSlewedPolyMelody[15][s] != pubB[s]) isoPub = false;
        }
        check(isoRaw, "V16 MELODY data unaffected by rhythm-mix change (random_)");
        check(isoPub, "V16 MELODY bars unaffected by rhythm-mix change (pubSlewed_)");
    }

    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
