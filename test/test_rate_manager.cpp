// test_rate_manager.cpp — CC_PROMPT_RATE_MANAGER_START: the compute-rate authority's
// contract suite (Stage 0: the lean core).
//
//   R1 TIER STATE: advance() sets the tick + bumps the generation only when fired;
//      ticked() reflects the last advance; a non-fired advance clears the tick.
//   R2 SKIP-ON-UNCHANGED (the SweepSig pattern, now the ONE change-detection
//      mechanism): a producer's first dispatch always runs; identical inputs on the
//      next tick → SKIPPED; any fingerprinted input perturbed → re-runs; static
//      again → skipped. A moving input re-runs every tick — correct.
//   R3 TIER DISCIPLINE: a producer declared at Control NEVER runs on another tier's
//      dispatch; dispatch is idempotent within one generation (at most once per tick).
//   R4 FINGERPRINT SEMANTICS: a size change alone counts as changed; the memcmp is
//      byte-exact (a one-bit float change re-runs).
//
// Header-only: the manager (ui/RateManager.hpp) is deliberately rack-free — the rig
// drives the REAL manager, never a copy (the ModuleLookupCore pattern).
#include "RateManager.hpp"
#include <cmath>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

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

// A fake producer's input set — the fingerprint MUST cover every field the produce
// reads (the bit-identity rule). Packed so the byte image has no padding surprises.
struct __attribute__((packed)) FakeInputs {
    float a = 1.f;
    float b = 2.f;
    int   n = 7;
    bool  flag = false;
};

int main() {
    std::cout << "\033[1mCompute-Rate Manager (CC_PROMPT_RATE_MANAGER_START, Stage 0) Contract Tests\033[0m\n"
              << std::string(74, '=') << "\n";

    TEST("R1 TIER STATE: advance fires the tick + bumps the generation; a non-fired advance clears the tick", {
        redDot::RateManager rm;
        EXPECT(!rm.ticked(redDot::Rate::Control));
        EXPECT(rm.generation(redDot::Rate::Control) == 0);
        rm.advance(redDot::Rate::Control, true);
        EXPECT(rm.ticked(redDot::Rate::Control));
        EXPECT(rm.generation(redDot::Rate::Control) == 1);
        rm.advance(redDot::Rate::Control, true);
        EXPECT(rm.generation(redDot::Rate::Control) == 2);
        rm.advance(redDot::Rate::Control, false);
        EXPECT(!rm.ticked(redDot::Rate::Control));
        EXPECT(rm.generation(redDot::Rate::Control) == 2);   // generation is monotonic
        // Tiers are independent state (independent clocks — identity, not ordering):
        rm.advance(redDot::Rate::Frame, true);
        EXPECT(rm.ticked(redDot::Rate::Frame));
        EXPECT(!rm.ticked(redDot::Rate::Control));
        EXPECT(rm.generation(redDot::Rate::Frame) == 1);
    });

    TEST("R2 SKIP-ON-UNCHANGED: first dispatch runs; identical inputs skip; a perturbed input re-runs; static skips again", {
        redDot::RateManager rm;
        FakeInputs in;
        int produced = 0;
        int id = rm.declareProducer(
            redDot::Rate::Control,
            /*fingerprint*/ [&](void* dst) {
                std::memcpy(dst, &in, sizeof in);
                return sizeof in;
            },
            /*produce*/ [&] { ++produced; });
        EXPECT(id == 0);
        // Tick 1: first-ever dispatch always runs (nothing to compare against).
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 1, "%d runs on first tick — must run once", produced);
        // Tick 2: identical inputs → SKIP (the published state is already correct).
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 1, "%d runs on an identical-input tick — the skip failed", produced);
        EXPECT(rm.runs(id) == 1);
        // Tick 3: perturb ONE input → re-runs.
        in.a = 1.5f;
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 2, "%d runs after perturbing input a — must re-run", produced);
        // Tick 4: a DIFFERENT input (the int) → re-runs.
        in.n = 8;
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 3, "%d runs after perturbing input n — every input must be fingerprinted", produced);
        // Tick 5: static again → skip.
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 3, "%d runs on a static tick — the skip failed", produced);
        // Tick 6: NO tick → dispatch is a no-op (the manager only dispatches on ticks).
        rm.advance(redDot::Rate::Control, false);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 3, "%d runs without a tick — dispatch must be inert", produced);
        // A moving input re-runs EVERY tick — correct (the value really changed).
        for (int i = 0; i < 5; ++i) {
            in.b += 0.25f;
            rm.advance(redDot::Rate::Control, true);
            rm.dispatch(redDot::Rate::Control);
        }
        EXPECTF(produced == 8, "%d runs over 5 moving ticks — must re-run each tick", produced);
    });

    TEST("R3 TIER DISCIPLINE: a Control producer never runs on Frame dispatches; dispatch is idempotent within a generation", {
        redDot::RateManager rm;
        int produced = 0;
        rm.declareProducer(
            redDot::Rate::Control,
            [](void* dst) { int v = 1; std::memcpy(dst, &v, sizeof v); return sizeof v; },
            [&] { ++produced; });
        // Frame ticks + dispatches never touch the Control producer.
        for (int i = 0; i < 10; ++i) {
            rm.advance(redDot::Rate::Frame, true);
            rm.dispatch(redDot::Rate::Frame);
        }
        EXPECTF(produced == 0, "%d runs from Frame dispatches — wrong-tier leak", produced);
        // One Control tick: dispatch twice — the producer runs AT MOST ONCE per tick.
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 1, "%d runs for a double dispatch in one generation — must be once", produced);
        // Next Control tick, identical inputs → still skipped.
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 1, "%d runs on identical inputs — the skip failed", produced);
        // Multiple producers at the same tier dispatch independently.
        int produced2 = 0;
        rm.declareProducer(
            redDot::Rate::Control,
            [](void* dst) { int v = 2; std::memcpy(dst, &v, sizeof v); return sizeof v; },
            [&] { ++produced2; });
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced2 == 1, "%d runs — the new producer's first tick must run", produced2);
        EXPECTF(produced == 1, "%d runs — the old producer (same inputs) must stay skipped", produced);
    });

    TEST("R4 FINGERPRINT SEMANTICS: byte-exact compare (a one-ULP float change re-runs); a size change counts as changed", {
        redDot::RateManager rm;
        FakeInputs in;
        int produced = 0;
        size_t snapSize = sizeof in;
        rm.declareProducer(
            redDot::Rate::Control,
            [&](void* dst) {
                std::memcpy(dst, &in, snapSize);
                return snapSize;
            },
            [&] { ++produced; });
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECT(produced == 1);
        // A one-ULP float change re-runs (memcmp is byte-exact, not epsilon).
        float next = in.a;
        next = std::nextafter(next, 100.f);
        in.a = next;
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 2, "%d runs after a one-ULP change — the compare must be byte-exact", produced);
        // Same values, but a SMALLER reported fingerprint size → changed (run), then a
        // matching-size shadow settles again.
        snapSize = sizeof in - 1;
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 3, "%d runs after a size change — size alone must count as changed", produced);
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 3, "%d runs on identical (resized) inputs — must skip", produced);
    });

    TEST("R5 SELF-HEALING IMAGE (the S0b shape): a fingerprint covering the produced cells re-runs after a FOREIGN write, re-asserts, then settles", {
        // The Stage-0 sync-population producer's fingerprint includes BOTH its inputs AND
        // the cells it writes — so an external write to those cells (the engine-reset
        // path resets the LOR store at runtime) is a fingerprint change: the producer
        // re-runs and re-asserts its values, exactly as the old unconditional per-tick
        // code did. This pins the MECHANISM on the real manager; the real block is a
        // verbatim extraction (bit-identity by construction + Rack feel-test).
        redDot::RateManager rm;
        struct State { float cell[4] = {1, 2, 3, 4}; float input = 0.5f; };
        State st;
        int produced = 0;
        rm.declareProducer(
            redDot::Rate::Control,
            [&](void* dst) {
                struct __attribute__((packed)) Img { float input; float cell[4]; };
                Img img{};
                img.input = st.input;
                std::memcpy(img.cell, st.cell, sizeof img.cell);
                std::memcpy(dst, &img, sizeof img);
                return sizeof img;
            },
            [&] { ++produced; for (auto& c : st.cell) c = 42.f; });   // "the population"
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECT(produced == 1);                                    // first tick runs
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 1, "%d runs on a static tick — must skip", produced);
        st.cell[2] = 7.f;                                         // a FOREIGN write (the reset shape)
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 2, "%d runs after a foreign write — the image must detect it", produced);
        EXPECTF(st.cell[2] == 42.f, "%f — the re-run must re-assert", st.cell[2]);
        rm.advance(redDot::Rate::Control, true);
        rm.dispatch(redDot::Rate::Control);
        EXPECTF(produced == 2, "%d runs after healing — must settle/skip", produced);
    });

    std::cout << std::string(74, '=') << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail == 0 ? 0 : 1;
}
