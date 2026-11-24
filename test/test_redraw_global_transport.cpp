// test_redraw_global_transport.cpp — Rodney's ruling (2026-11-24): the RE-DRAW (the roll / draw-
// counter advance / new random numbers / the slew-consume boundary) is driven by GLOBAL TRANSPORT
// in ALL cases. This retires the "effective draw-window lap" boundary that CC_PROMPT_UAS_CYCLE_
// PHRASE_MODE contract #9 and CC_PROMPT_SCRUB32 Part A (amended) had floated for Cycle:Custom.
//
// Why this holds by construction (verified in code 2026-11-24): the transport wrap is
// `wrapped = pos_.phrasePos()==0` at `cachedLength`, and `cachedLength` is set only by
// setWindow <- updateWindow from the GLOBAL length control; the draw counter advances by +/-1 per
// roll (advance*Draw) with NO window-length term; and a re-draw happens only in redraw* inside
// applyPendingSeedsAndRedraw, which runs only at the phrase boundary. A per-voice/lane window
// length (LOR_LEN) therefore CANNOT change the re-draw cadence. These tests pin that so the future
// Cycle:Custom wiring cannot introduce a per-draw-window-lap re-draw.
#include "test_stubs.hpp"
#include "engines/SequencerEngine.hpp"
#include <iostream>
#include <cstdio>

static int s_pass = 0, s_fail = 0;
#define TEST(desc, ...) do { \
    bool _ok = true; std::string _m; \
    try { __VA_ARGS__; } catch (const std::exception& _e) { _ok = false; _m = _e.what(); } \
    if (_ok) { ++s_pass; std::cout << "  \033[32m✓\033[0m " << (desc) << "\n"; } \
    else { ++s_fail; std::cout << "  \033[31m✗\033[0m " << (desc); if(!_m.empty()) std::cout << " — " << _m; std::cout << "\n"; } \
} while (0)
#define EXPECT(e) do { if (!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while (0)

// Drive `steps` transport steps; in realtime mode each phrase-boundary wrap re-draws. Return the
// number of GLOBAL wraps seen and how far the rhythm draw counter advanced.
struct DriveResult { int wraps; long long drawDelta; };
static DriveResult drive(SequencerEngine& eng, int steps) {
    PatternInput in;
    eng.pe.rhythmMode = 1;                        // realtime: a wrap re-draws
    // Warm up past the initial SEED draw: the first boundary re-draw is the seed (redraw* runs with
    // first==true, which does NOT advance the draw counter). Counting from AFTER it, each global
    // wrap advances the counter exactly once.
    for (int s = 0; s < 64; ++s) {
        if (eng.advancePlayhead(1, nullptr)) { eng.pe.applyPendingSeedsAndRedraw(in); break; }
    }
    const long long ctr0 = eng.pe.rhythmDrawCtr;
    int wraps = 0;
    for (int s = 0; s < steps; ++s) {
        const bool wrapped = eng.advancePlayhead(1, nullptr);
        if (wrapped) { eng.pe.applyPendingSeedsAndRedraw(in); ++wraps; }
    }
    return { wraps, (long long)eng.pe.rhythmDrawCtr - ctr0 };
}

static SequencerEngine* mk(float globalLen, int laneLen) {
    auto* e = new SequencerEngine();
    e->reset();
    e->updateWindow(globalLen, 0.f, false, 0.f, 0.f, false);   // GLOBAL transport length
    e->pe.seedRhythmPhilox(0.42f);
    if (laneLen > 0) e->lorRef(0, SequencerEngine::LOR_LEN) = laneLen;   // a lane window != global
    return e;
}

int main() {
    std::cout << "\033[1mRe-draw follows global transport (Rodney 2026-11-24)\033[0m\n" << std::string(52, '=') << "\n";

    // T1 — re-draw ONCE per global transport wrap (not per the lane window).
    TEST("one re-draw per global wrap; cadence is the global length, not the len-7 lane", {
        SequencerEngine* e = mk(16.f, 7);
        auto r = drive(*e, 48);
        std::printf("      [global 16, lane 7, 48 steps] wraps=%d drawDelta=%lld\n", r.wraps, r.drawDelta);
        EXPECT(r.drawDelta == r.wraps);          // re-draw exactly once per global wrap
        EXPECT(r.wraps >= 2 && r.wraps <= 4);    // ~48/16 — the GLOBAL cadence, not 48/7
        delete e;
    });

    // T2 — the re-draw cadence is independent of the lane/voice window length.
    TEST("same global length, different lane windows (7 vs 13) -> identical re-draw cadence", {
        SequencerEngine* a = mk(16.f, 7);
        SequencerEngine* b = mk(16.f, 13);
        auto ra = drive(*a, 64), rb = drive(*b, 64);
        std::printf("      [global 16] lane7: wraps=%d draw=%lld | lane13: wraps=%d draw=%lld\n",
                    ra.wraps, ra.drawDelta, rb.wraps, rb.drawDelta);
        EXPECT(ra.wraps == rb.wraps);
        EXPECT(ra.drawDelta == rb.drawDelta);
        EXPECT(ra.drawDelta == ra.wraps);
        delete a; delete b;
    });

    // T3 — CONTROL: the cadence really IS the global length (change it, the cadence changes).
    TEST("a shorter global length (10) re-draws more often than 16 over the same steps", {
        SequencerEngine* e16 = mk(16.f, 0);
        SequencerEngine* e10 = mk(10.f, 0);
        auto r16 = drive(*e16, 80), r10 = drive(*e10, 80);
        std::printf("      [80 steps] global16: wraps=%d draw=%lld | global10: wraps=%d draw=%lld\n",
                    r16.wraps, r16.drawDelta, r10.wraps, r10.drawDelta);
        EXPECT(r10.wraps > r16.wraps);                       // 80/10 > 80/16 — cadence tracks global length
        EXPECT(r16.drawDelta == r16.wraps && r10.drawDelta == r10.wraps);
        delete e16; delete e10;
    });

    std::cout << std::string(52, '=') << "\n"
              << (s_fail == 0 ? "\033[32m" : "\033[31m")
              << s_pass << " passed, " << s_fail << " failed\033[0m\n";
    return s_fail == 0 ? 0 : 1;
}
