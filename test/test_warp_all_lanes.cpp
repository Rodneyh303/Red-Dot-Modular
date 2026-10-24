// test_warp_all_lanes.cpp — CC_PROMPT_05: ONE uniform source (warpRow) for EVERY lane × voice.
//
// The principle under contract: there is EXACTLY ONE way any consumer gets a step's
// uniform — warpRow(v, lane). No per-lane (MEL||OCT) gate, no displayedVoice_==0
// mono/poly branch, no direct polyRandom/polyRandomSrc/random_ read in any consumer
// (threshold, pickSemitone, pitch, CV out, display). One path ⇒ "lane is special /
// voice is special" divergence is impossible by construction.
//
// Sections (the prompt's contract list):
//   A1. DEFAULT NO-OP: IdentityWarp + w=0 (a default bank wired) ⇒ the consumer
//       uniform == raw random_ BYTE-IDENTICAL for ALL 112 (voice,lane) cells
//       (16 voices × 7 lanes), through the consumer accessors — the CV-out
//       funnel (polyLaneProbabilityAtStep) and the display funnel
//       (finalRandomByStrand) — plus the bank-less twin. Proves the uniform
//       plumbing changed NOTHING at defaults. (Green before AND after — the
//       no-behaviour-change proof; the other sections are the RED→GREEN pins.)
//   A2. WARP REACHES: non-identity WarpF + w>0 on ONE (voice,lane) ⇒ that cell's
//       consumer uniform == the mirror value (≠ raw) while every OTHER cell is
//       unchanged. Pinned on a BOOLEAN lane (REST) — the lane the old gate left raw.
//   A3. NO RAW LEAK: warp authored on EVERY (voice,lane) ⇒ EVERY lane's consumer
//       path returns the mirror (CV-outs for all 7 lanes × 16 voices; display
//       funnel for all 7 strands). A bank being wired must be the ONLY thing
//       that changes what a consumer reads.
//   A4. DISPLAY/DRAW SITES: redrawRhythm's rhythmPattern (the rhythm redraw twin)
//       and refreshVisualCache's display derivation (the collapsed mono/poly
//       ternary) both read warpRow(displayedVoice_, PL_REST).
//   A5. STRUCTURAL no-raw-leak (the "grep in test"): the gated seam polyRandomSrc
//       is GONE from the engine/manager/visual sources, the mono-feeder raw reads
//       are gone, the display ternary + draw-twin raw reads are gone — warpRow is
//       the sole consumer uniform source. (RED while any site still reads raw.)
//
// Companion sources: PatternEngine.cpp + SequencerEngine.cpp + GateState.cpp
// (the test_warp_mirror pattern).
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "WarpF.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static int s_pass = 0, s_fail = 0;
#define TEST(desc, ...) do { \
    bool _ok = true; std::string _msg; \
    try { __VA_ARGS__; } catch (const std::exception& _e) { _ok = false; _msg = _e.what(); } \
    if (_ok) { ++s_pass; std::cout << "  \033[32m✓\033[0m " << (desc) << "\n"; } \
    else { ++s_fail; std::cout << "  \033[31m✗\033[0m " << (desc); \
           if (!_msg.empty()) std::cout << " — " << _msg; std::cout << "\n"; } \
} while (0)
#define EXPECT(e) do { if (!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while (0)
#define EXPECTF(e, ...) do { if (!(e)) { char _b[256]; std::snprintf(_b, sizeof _b, __VA_ARGS__); \
    throw std::runtime_error(std::string("EXPECT(" #e ") failed: ") + _b); } } while (0)

// Deterministic, well-spread [0,1) values across the whole store (golden-ratio
// scramble) — same generator as test_warp_mirror so the suites share a basis.
static void fillRandomGolden(PatternEngine& pe) {
    for (int v = 0; v < 16; ++v)
        for (int l = 0; l < 7; ++l)
            for (int s = 0; s < 16; ++s) {
                const long idx = (long)v * 7 * 16 + l * 16 + s;
                pe.random_[v][l][s] = (float)std::fmod(0.125 + idx * 0.6180339887498949, 1.0);
            }
    pe.random_[0][0][0] = 0.f;
    pe.random_[0][0][1] = 1e-7f;
    pe.random_[15][6][15] = 1.f - 1e-7f;
}

// ── A5 helpers: the structural "grep in test" ─────────────────────────────────
static std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}
static size_t countOccurrences(const std::string& hay, const std::string& needle) {
    size_t n = 0, pos = 0;
    while ((pos = hay.find(needle, pos)) != std::string::npos) { ++n; pos += needle.size(); }
    return n;
}
static std::string repoRoot() {
    static const char* candidates[] = { ".", "..", "../.." };
    for (const char* m : candidates) {
        std::ifstream f(std::string(m) + "/src/dsp/engines/SequencerEngine.cpp", std::ios::binary);
        if (f) return m;
    }
    throw std::runtime_error("repo root not found — run via test/run_all.sh from the repo");
}

int main() {
    std::cout << "\033[1mWarp ALL lanes/voices (CC_PROMPT_05) Tests\033[0m\n" << std::string(56, '=') << "\n";
    using dotModular::WarpBank;
    const int MEL = PatternEngine::PL_MELODY, OCT = PatternEngine::PL_OCTAVE,
              QMIX = PatternEngine::PL_QMIX, REST = PatternEngine::PL_REST,
              ACC = PatternEngine::PL_ACCENT, VAR = PatternEngine::PL_VARIATION,
              LEG = PatternEngine::PL_LEGATO;
    (void)MEL; (void)OCT; (void)QMIX; (void)ACC; (void)VAR; (void)LEG;

    // ── A1: DEFAULT NO-OP — all 112 (voice,lane) rows, byte-identical ───────────
    TEST("A1 default no-op (bank wired, IdentityF + w=0): CV-out consumer == raw for ALL 112 (voice,lane) rows, byte-identical", {
        SequencerEngine se;
        fillRandomGolden(se.pe);
        WarpBank bank;                 // the Monsoon default: all-Identity, w=0
        se.pe.setWarpBank(&bank);      // wires + full rebuild
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                for (int s = 0; s < 16; ++s) {
                    const float c = se.polyLaneProbabilityAtStep(dotModular::LaneId{l}, dotModular::VoiceResolver::slot(v + 1), s);
                    EXPECTF(std::memcmp(&c, &se.pe.random_[v][l][s], sizeof(float)) == 0,
                            "cell (v=%d,l=%d,s=%d): consumer %.9g != raw %.9g",
                            v, l, s, (double)c, (double)se.pe.random_[v][l][s]);
                }
        // The display funnel (voice 0) for every strand:
        for (int st = 0; st < dotModular::NUM_STRANDS; ++st)
            for (int s = 0; s < 16; ++s)
                EXPECTF(se.pe.finalRandomByStrand(st, s) == se.pe.random_[0][st][s],
                        "strand %d step %d", st, s);
    });

    TEST("A1 default no-op (bank-less twin): consumer == raw for ALL 112 rows (legacy byte-identity)", {
        SequencerEngine se;
        fillRandomGolden(se.pe);       // no bank wired
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                for (int s = 0; s < 16; ++s)
                    EXPECT(se.polyLaneProbabilityAtStep(dotModular::LaneId{l}, dotModular::VoiceResolver::slot(v + 1), s) == se.pe.random_[v][l][s]);
        for (int st = 0; st < dotModular::NUM_STRANDS; ++st)
            for (int s = 0; s < 16; ++s)
                EXPECT(se.pe.finalRandomByStrand(st, s) == se.pe.random_[0][st][s]);
    });

    // ── A2: WARP REACHES one (voice,lane) — pinned on a BOOLEAN lane ────────────
    TEST("A2 warp reaches (5, REST): that row's consumer == mirror (≠ raw); every other cell unchanged", {
        SequencerEngine se;
        fillRandomGolden(se.pe);
        WarpBank bank; se.pe.setWarpBank(&bank);
        bank.warpF[WarpBank::index(5, REST)] = dotModular::TriangularWarp{0.25};
        bank.warpW[WarpBank::index(5, REST)] = 1.f;
        se.pe.rebuildWarpMirrorAll();
        int rowDiffs = 0;
        for (int s = 0; s < 16; ++s) {
            const float c = se.polyLaneProbabilityAtStep(dotModular::LaneId{REST}, dotModular::VoiceResolver::slot(6), s);
            EXPECTF(c == se.pe.warped_[5][REST][s],
                    "step %d: consumer %.9g != mirror %.9g (raw %.9g)",
                    s, (double)c, (double)se.pe.warped_[5][REST][s], (double)se.pe.random_[5][REST][s]);
            if (c != se.pe.random_[5][REST][s]) ++rowDiffs;
        }
        EXPECTF(rowDiffs > 0, "vacuous: warp moved no cell in the (5,REST) row");
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l) {
                if (v == 5 && l == REST) continue;
                for (int s = 0; s < 16; ++s)
                    EXPECTF(se.polyLaneProbabilityAtStep(dotModular::LaneId{l}, dotModular::VoiceResolver::slot(v + 1), s) == se.pe.random_[v][l][s],
                            "collateral change at (v=%d,l=%d,s=%d)", v, l, s);
            }
    });

    // ── A3: NO RAW LEAK — warp on EVERY (voice,lane) ⇒ every consumer reads the mirror ──
    TEST("A3 no raw leak: warp authored on ALL 112 rows ⇒ CV-out consumer == mirror for every lane × voice × step", {
        SequencerEngine se;
        fillRandomGolden(se.pe);
        WarpBank bank; se.pe.setWarpBank(&bank);
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l) {
                bank.warpF[WarpBank::index(v, l)] = dotModular::TriangularWarp{0.25};
                bank.warpW[WarpBank::index(v, l)] = 1.f;
            }
        se.pe.rebuildWarpMirrorAll();
        int diffs = 0;
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                for (int s = 0; s < 16; ++s) {
                    const float c = se.polyLaneProbabilityAtStep(dotModular::LaneId{l}, dotModular::VoiceResolver::slot(v + 1), s);
                    EXPECTF(c == se.pe.warped_[v][l][s],
                            "RAW LEAK at (v=%d,l=%d,s=%d): consumer %.9g != mirror %.9g (raw %.9g)",
                            v, l, s, (double)c, (double)se.pe.warped_[v][l][s], (double)se.pe.random_[v][l][s]);
                    if (c != se.pe.random_[v][l][s]) ++diffs;
                }
        EXPECTF(diffs > 1000, "vacuous: warp moved only %d of 1792 cells", diffs);
    });

    TEST("A3 no raw leak: display funnel returns the mirror for ALL 7 strands (warp authored on every row)", {
        PatternEngine pe;
        fillRandomGolden(pe);
        WarpBank bank; pe.setWarpBank(&bank);
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l) {
                bank.warpF[WarpBank::index(v, l)] = dotModular::TriangularWarp{0.25};
                bank.warpW[WarpBank::index(v, l)] = 1.f;
            }
        pe.rebuildWarpMirrorAll();
        int diffs = 0;
        for (int st = 0; st < dotModular::NUM_STRANDS; ++st)
            for (int s = 0; s < 16; ++s) {
                const float c = pe.finalRandomByStrand(st, s);
                EXPECTF(c == pe.warped_[0][st][s],
                        "RAW LEAK at (strand=%d, s=%d): display %.9g != mirror %.9g (raw %.9g)",
                        st, s, (double)c, (double)pe.warped_[0][st][s], (double)pe.random_[0][st][s]);
                if (c != pe.random_[0][st][s]) ++diffs;
            }
        EXPECTF(diffs > 90, "vacuous: warp moved only %d of 112 display cells", diffs);
    });

    // ── A4: DISPLAY/DRAW SITES — the rhythm redraw twin + the collapsed ternary ──
    auto rhythmInput = [] {
        PatternInput in;
        in.restProb = 0.35f;
        return in;
    };

    TEST("A4 draw twin: redrawRhythm derives rhythmPattern from warpRow(·, PL_REST), not raw slewed[0]", {
        const PatternInput in = rhythmInput();
        PatternEngine pe;
        pe.seedRhythmPhilox(0.42f);
        pe.latchMix(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, /*R=*/true, /*M=*/false, /*Q=*/false);
        WarpBank bank;
        bank.warpF[WarpBank::index(0, REST)] = dotModular::PowerWarp{3};
        bank.warpW[WarpBank::index(0, REST)] = 1.f;
        pe.setWarpBank(&bank);
        pe.redrawRhythm(in);
        for (int i = 0; i < 16; ++i)
            EXPECTF(pe.rhythmPattern[i] == (pe.warpRow(dotModular::VoiceResolver::slot(0 + 1), dotModular::LaneId{REST})[i] >= in.restProb),
                    "step %d: pattern %d != warpRow-fed %d (warped %.9g vs restProb %.9g)",
                    i, (int)pe.rhythmPattern[i], (int)(pe.warpRow(dotModular::VoiceResolver::slot(0 + 1), dotModular::LaneId{REST})[i] >= in.restProb),
                    (double)pe.warpRow(dotModular::VoiceResolver::slot(0 + 1), dotModular::LaneId{REST})[i], (double)in.restProb);
        // Non-vacuous: the warp must move at least one cell across the threshold
        // vs what the RAW row would have produced (the pre-05 expression).
        int diffs = 0;
        for (int i = 0; i < 16; ++i)
            if (pe.rhythmPattern[i] != (pe.random_[0][REST][i] >= in.restProb)) ++diffs;
        EXPECTF(diffs > 0, "vacuous: warp moved no cell across the rest threshold");
    });

    TEST("A4 display ternary collapsed: refreshVisualCache derives rhythmPattern from warpRow(displayedVoice_, PL_REST)", {
        const PatternInput in = rhythmInput();
        PatternEngine pe;
        pe.numPolyVoicesHint = 5;
        pe.displayedVoice_ = dotModular::VoiceResolver::slot(4);   // V4 = slot 3, a POLY displayed voice — the old ternary's other arm
        pe.seedRhythmPhilox(0.42f);
        pe.seedMelodyPhilox(0.17f);
        pe.seedQmixPhilox(0.23f);
        pe.latchMix(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, /*R=*/true, /*M=*/true, /*Q=*/true);
        WarpBank bank;
        bank.warpF[WarpBank::index(3, REST)] = dotModular::PowerWarp{3};
        bank.warpW[WarpBank::index(3, REST)] = 1.f;
        pe.setWarpBank(&bank);
        fillRandomGolden(pe);        // deterministic basis; the recompute re-derives REST rows
        pe.rebuildWarpMirrorAll();
        // Force the rhythmChanged display path (counter != applied ⇒ full recompute
        // + the :886-889 rhythmPattern derivation).
        pe.rhythmCtrApplied = pe.rhythmDrawCtr - 1;
        pe.refreshVisualCache(in);
        EXPECT(pe.rhythmDrawCtr == pe.rhythmCtrApplied);   // the recompute consumed it
        for (int i = 0; i < 16; ++i)
            EXPECTF(pe.rhythmPattern[i] == (pe.warpRow(dotModular::VoiceResolver::slot(3 + 1), dotModular::LaneId{REST})[i] >= in.restProb),
                    "step %d: pattern %d != warpRow(3,REST)-fed %d (warped %.9g)",
                    i, (int)pe.rhythmPattern[i], (int)(pe.warpRow(dotModular::VoiceResolver::slot(3 + 1), dotModular::LaneId{REST})[i] >= in.restProb),
                    (double)pe.warpRow(dotModular::VoiceResolver::slot(3 + 1), dotModular::LaneId{REST})[i]);
        int diffs = 0;
        for (int i = 0; i < 16; ++i)
            if (pe.rhythmPattern[i] != (pe.random_[3][REST][i] >= in.restProb)) ++diffs;
        EXPECTF(diffs > 0, "vacuous: warp moved no cell across the rest threshold");
    });

    // ── A5: STRUCTURAL no-raw-leak — the gated seam + raw read patterns are GONE ──
    TEST("A5 structural: polyRandomSrc gone from every engine/manager/visual source; mono-feeder + display/draw raw reads gone", {
        const std::string root = repoRoot();
        struct Probe { const char* file; const char* needle; };
        const Probe probes[] = {
            // The gated consumer seam itself — deleted by the uniform plumbing.
            { "src/dsp/engines/SequencerEngine.hpp", "polyRandomSrc" },
            { "src/dsp/engines/SequencerEngine.cpp", "polyRandomSrc" },
            { "src/dsp/engines/PatternEngine.hpp",   "polyRandomSrc" },
            { "src/dsp/managers/SpreadManager.hpp",  "polyRandomSrc" },
            { "src/StraitsEastSandsVisual.cpp",      "polyRandomSrc" },
            { "src/StraitsSandsMacroVisual.cpp",     "polyRandomSrc" },
            // The mono-feeder raw reads (executeModeA/B/Subdivided + checkpoint + ghost).
            { "src/dsp/engines/SequencerEngine.hpp", "pe.polyRandom(0, PL_" },
            { "src/dsp/engines/SequencerEngine.cpp", "pe.polyRandom(0, PL_" },
            // The display ternary's raw poly arm + the draw twin's raw slewed read.
            { "src/dsp/engines/PatternEngine.cpp",   "polyRandom(displayedVoice_" },
            { "src/dsp/engines/PatternEngine.cpp",   "slewedRhythm[0][i] >= in.restProb" },
            // CC_PROMPT_STRONG_TYPES tighten: the BARE-INT per-voice/lane paths are GONE —
            // every consumer mints a VoiceSlot (mono() for the anchor role) and passes an
            // explicit LaneId. A regressed bare index is a compile error AND this pin.
            { "src/dsp/engines/PatternEngine.hpp",   "warpRow(0, " },
            { "src/dsp/engines/PatternEngine.cpp",   "warpRow(0, PL_" },
            { "src/dsp/engines/PatternEngine.cpp",   "polyRandom(0, PL_" },
            { "src/dsp/engines/PatternEngine.hpp",   "voices[0]." },
            { "src/dsp/engines/SequencerEngine.hpp", "voices[0]." },
            { "src/dsp/engines/SequencerEngine.cpp", "voices[0]." },
            { "src/dsp/engines/SequencerEngine.cpp", "pe.warpRow(0" },
        };
        for (const Probe& p : probes) {
            const std::string src = slurp(root + "/" + p.file);
            const size_t n = countOccurrences(src, p.needle);
            EXPECTF(n == 0, "%s still contains %zu × \"%s\" (raw consumer read survives)",
                    p.file, n, p.needle);
        }
    });

    std::cout << "-----\n";
    std::printf("warp all lanes (CC_PROMPT_05): %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
