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
#include <filesystem>
#include <fstream>
#include <iostream>
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

    TEST("R6 INTERNAL SR-AWARE CLOCKS (the S7 shape): setSampleRate + tickClocks — Control every round(SR/1500) samples, Frame every round(SR/90); raising SR scales only the sample divisor (same times/SECOND)", {
        redDot::RateManager rm;
        rm.setSampleRate(48000.f);   // Control /32, Frame /533 (the old divider divisions)
        int ctrl = 0, frame = 0;
        for (int i = 0; i < 32000; ++i) {          // 2/3 s @ 48k
            rm.tickClocks();
            if (rm.ticked(redDot::Rate::Control)) ++ctrl;
            if (rm.ticked(redDot::Rate::Frame))   ++frame;
        }
        EXPECTF(ctrl == 1000, "%d Control ticks in 32000 samples @48k — want 1000 (/32)", ctrl);
        EXPECTF(frame == 60,  "%d Frame ticks in 32000 samples @48k — want ~60 (/533)", frame);
        // SR-UP: the SAME one-second wall time → the SAME tick counts (only the Sample
        // tier scales with SR — the CPU invariant S9 pins per-second).
        redDot::RateManager rm2;
        rm2.setSampleRate(96000.f);   // Control /64, Frame /1067
        int ctrl2 = 0, frame2 = 0;
        for (int i = 0; i < 96000; ++i) {          // 1 s @ 96k
            rm2.tickClocks();
            if (rm2.ticked(redDot::Rate::Control)) ++ctrl2;
            if (rm2.ticked(redDot::Rate::Frame))   ++frame2;
        }
        EXPECTF(ctrl2 == 1500, "%d Control ticks in 1 s @96k — want 1500 (same per-second)", ctrl2);
        // Frame @96k: division = round(96000/90) = 1067 → floor(96000/1067) = 89 ticks
        // in the first second (89.97 Hz — the integer-division quantization the old
        // lightDivider had too; the 90th lands 30 samples into second two).
        EXPECTF(frame2 == 89,  "%d Frame ticks in 1 s @96k — want 89 (÷1067 quantization)", frame2);
        // The Phase/Step tiers are NOT touched by tickClocks (independent clocks):
        EXPECT(!rm.ticked(redDot::Rate::Step));
        EXPECT(!rm.ticked(redDot::Rate::PPQN));
    });

    TEST("R7 STRUCTURAL grep-gate: no raw sequencing-rate primitives outside the authority (FATAL by default; RATE_MANAGER_STRICT=0 = local debug hatch)", {
        // The enforcement backstop (deletion is the braces; this is the belt — the
        // module-lookup L5 precedent). Forbidden, outside the allow-listed sources:
        //   1. "ClockDivider" in Monsoon.hpp + dsp/engines/ — the in-scope raw divider
        //      MEMBERS (deleted in S7; a deleted member cannot be .process()'d).
        //   2. "gridPulse" anywhere — the S3a deletion (the PPQN tier owns it).
        //   3. ".pulseEdge" / ".sixteenthEdge" / ".quarterEdge" outside the SOURCE
        //      engines (ClockEngine/PhaseEngine compute them) and Monsoon.cpp (the ONE
        //      tier-feed site: advance(Rate::PPQN, <source edge>)) and this test.
        //   4. "% controlRate" anywhere — a hand-rolled control-rate modulo.
        // ALLOWED by design (documented dispositions): the expander wrapper FEEDS
        // (gateModDiv/probOutDiv on the two Sands visuals — the /8-exact detector
        // ruling + the /32 recompute feeds; their .process() calls feed manager tiers).
        static const char* const edgeAllowFiles[] = {
            "src/dsp/engines/ClockEngine.hpp", "src/dsp/engines/ClockEngine.cpp",
            "src/dsp/engines/PhaseEngine.hpp", "src/dsp/engines/PhaseEngine.cpp",
            "src/Monsoon.cpp",                // the tier feed (advance's source argument)
            "test/test_rate_manager.cpp",     // this gate
        };
        auto isEdgeAllowed = [&](const std::string& rel) {
            for (const char* a : edgeAllowFiles) if (rel == a) return true;
            return false;
        };
        struct Residual { std::string file; int line; };
        std::vector<Residual> residuals;
        for (const auto& entry : std::filesystem::recursive_directory_iterator("src")) {
            if (!entry.is_regular_file()) continue;
            const std::string ext = entry.path().extension().string();
            if (ext != ".cpp" && ext != ".hpp" && ext != ".ipp") continue;
            const std::string rel = std::filesystem::relative(entry.path()).generic_string();
            std::ifstream f(entry.path());
            std::string line;
            int lineNo = 0;
            while (std::getline(f, line)) {
                ++lineNo;
                if (line.find("RATE-MANAGER-GATE") != std::string::npos) continue;  // the ack marker
                if ((rel == "src/Monsoon.hpp"
                     || rel.rfind("src/dsp/engines/", 0) == 0)
                    && line.find("ClockDivider") != std::string::npos)
                    residuals.push_back({rel, lineNo});
                if (line.find("gridPulse") != std::string::npos)
                    residuals.push_back({rel, lineNo});
                if (!isEdgeAllowed(rel)
                    && (line.find(".pulseEdge") != std::string::npos
                        || line.find(".sixteenthEdge") != std::string::npos
                        || line.find(".quarterEdge") != std::string::npos))
                    residuals.push_back({rel, lineNo});
                if (line.find("% controlRate") != std::string::npos)
                    residuals.push_back({rel, lineNo});
            }
        }
        std::cout << "      residuals: " << residuals.size() << " raw rate-primitive line(s) outside the authority"
                  << " (expect 0 — deletion is the braces, this gate is the belt)" << std::endl;
        for (size_t i = 0; i < residuals.size() && i < 12; ++i)
            std::cout << "        " << residuals[i].file << ":" << residuals[i].line << "\n";
        // FATAL BY DEFAULT (the module-lookup L5 precedent): RATE_MANAGER_STRICT=0 is a
        // LOCAL debug hatch only (never CI).
        const char* relax = std::getenv("RATE_MANAGER_STRICT");
        const bool enforce = !(relax && relax[0] == '0');
        if (enforce)
            EXPECTF(residuals.empty(), "%zu raw rate-primitive sites remain outside the authority", residuals.size());
    });

    std::cout << std::string(74, '=') << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail == 0 ? 0 : 1;
}
