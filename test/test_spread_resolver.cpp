// test_spread_resolver.cpp ─ Step 3 of the probability-modifier unification.
//
// PURPOSE: the resolver + its regression oracle for "effective spread amount". The amount was
// computed inline at ≥3 sites in MonsoonSandsManager with the same arithmetic copied by hand;
// SpreadResolver::effective() is now the one definition. These tests pin that arithmetic against
// hand-computed values matching the CURRENT manager behaviour, so routing the manager through the
// resolver (build-verified in Rack) is provably behaviour-preserving, and the later spread[16][6]
// engine migration has a net.
//
// Pure floats/bools in → one float out, so this builds and runs OUTSIDE Rack (no SDK needed):
//   g++ -std=c++17 -I../src/dsp test_spread_resolver.cpp -o /tmp/tsr && /tmp/tsr

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include "SpreadResolver.hpp"

using redDot::SpreadResolver;

static int g_pass = 0, g_fail = 0;
#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT_NEAR(a,b) do { double _d=(double)(a)-(double)(b); if(_d<0)_d=-_d; if(_d>1e-6){ \
    std::ostringstream _s; _s << "EXPECT_NEAR(" #a "," #b ") : " << (a) << " != " << (b); \
    throw std::runtime_error(_s.str()); } } while(0)

static SpreadResolver::CvTerm cv(bool c, float unitV, float att) {
    SpreadResolver::CvTerm t; t.connected = c; t.unitVoltage = unitV; t.atten = att; return t;
}

int main() {
    SUITE("Base only (no CV, no macro)");
    TEST("base passes through unchanged", {
        SpreadResolver::Inputs in; in.base = 0.4f;
        EXPECT_NEAR(SpreadResolver::effective(in), 0.4f);
    });
    TEST("negative base preserved (spread is bipolar, NOT floored at 0)", {
        SpreadResolver::Inputs in; in.base = -0.7f;
        EXPECT_NEAR(SpreadResolver::effective(in), -0.7f);
    });

    SUITE("Own CV: base + cv·att·2 (the ×2 bipolar span)");
    TEST("full-depth CV (+1V unit, att 1) adds the full span reach", {
        SpreadResolver::Inputs in; in.base = 0.f; in.ownCv = cv(true, 0.5f, 1.f);
        // 0.5 * 1 * 2 = 1.0
        EXPECT_NEAR(SpreadResolver::effective(in), 1.f);
    });
    TEST("attenuverter scales and can invert", {
        SpreadResolver::Inputs in; in.base = 0.2f; in.ownCv = cv(true, 0.3f, -0.5f);
        // 0.2 + (0.3 * -0.5 * 2) = 0.2 - 0.3 = -0.1
        EXPECT_NEAR(SpreadResolver::effective(in), -0.1f);
    });
    TEST("unconnected own CV contributes nothing", {
        SpreadResolver::Inputs in; in.base = 0.25f; in.ownCv = cv(false, 9.9f, 1.f);
        EXPECT_NEAR(SpreadResolver::effective(in), 0.25f);
    });

    SUITE("East CV add (mono V1 spread mod, additive, same ×2 span)");
    TEST("East CV adds on top of base + own CV", {
        SpreadResolver::Inputs in; in.base = 0.1f;
        in.ownCv  = cv(true, 0.1f, 1.f);   // +0.2
        in.eastCv = cv(true, 0.15f, 1.f);  // +0.3
        // 0.1 + 0.2 + 0.3 = 0.6
        EXPECT_NEAR(SpreadResolver::effective(in), 0.6f);
    });

    SUITE("Macro send add (tapped delta, only when Macro present)");
    TEST("macro send adds macroSendDelta·send when present", {
        SpreadResolver::Inputs in; in.base = 0.2f; in.macroPresent = true;
        in.macroSendDelta = 0.5f; in.macroSend = 0.4f;
        // 0.2 + 0.5*0.4 = 0.4
        EXPECT_NEAR(SpreadResolver::effective(in), 0.4f);
    });
    TEST("macro send ignored when macro absent (even if delta/send set)", {
        SpreadResolver::Inputs in; in.base = 0.2f; in.macroPresent = false;
        in.macroSendDelta = 0.5f; in.macroSend = 0.4f;
        EXPECT_NEAR(SpreadResolver::effective(in), 0.2f);
    });

    SUITE("Clamp ONCE at the end (not per-term)");
    TEST("sum beyond +1 clamps to +1", {
        SpreadResolver::Inputs in; in.base = 0.8f; in.ownCv = cv(true, 0.5f, 1.f); // +1.0 → 1.8
        EXPECT_NEAR(SpreadResolver::effective(in), 1.f);
    });
    TEST("sum below -1 clamps to -1", {
        SpreadResolver::Inputs in; in.base = -0.8f; in.ownCv = cv(true, 0.5f, -1.f); // -1.0 → -1.8
        EXPECT_NEAR(SpreadResolver::effective(in), -1.f);
    });
    TEST("STEP-WISE clamp matches manager: intermediate overflow IS clamped before next term", {
        // base 0.9 + own 0.4 → 1.3, CLAMPED to 1.0 (step-wise, as the manager does), then
        // east -0.6 → 0.4. (A single end-clamp would give 0.9+0.4-0.6=0.7 — this asserts the
        // resolver replicates the manager's per-term clamp, i.e. 0.4, not 0.7.)
        SpreadResolver::Inputs in; in.base = 0.9f;
        in.ownCv  = cv(true, 0.2f, 1.f);   // +0.4  → 1.3 → clamp 1.0
        in.eastCv = cv(true, 0.3f, -1.f);  // -0.6  → 0.4
        EXPECT_NEAR(SpreadResolver::effective(in), 0.4f);
    });

    SUITE("Delegation override (lane ceded to Macro → mirror Macro, ignore own base/CV)");
    TEST("delegated lane returns clamp(macroBase + macroSendDelta), ignoring base & CV", {
        SpreadResolver::Inputs in;
        in.delegated = true; in.base = 0.9f;           // own base must be IGNORED
        in.ownCv = cv(true, 0.5f, 1.f);                 // own CV must be IGNORED
        in.macroBase = 0.3f; in.macroSendDelta = 0.2f;  // → 0.5
        EXPECT_NEAR(SpreadResolver::effective(in), 0.5f);
    });
    TEST("delegated result is clamped", {
        SpreadResolver::Inputs in;
        in.delegated = true; in.macroBase = 0.8f; in.macroSendDelta = 0.6f;  // 1.4 → 1.0
        EXPECT_NEAR(SpreadResolver::effective(in), 1.f);
    });

    SUITE("Composite: the full non-delegated mono path (base+ownCV+eastCV+macroSend)");
    TEST("all four contributions combine then clamp — matches the manager loop arithmetic", {
        SpreadResolver::Inputs in;
        in.base = 0.1f;
        in.ownCv  = cv(true, 0.05f, 1.f);   // +0.1
        in.eastCv = cv(true, 0.05f, 1.f);   // +0.1
        in.macroPresent = true; in.macroSendDelta = 0.4f; in.macroSend = 0.5f;  // +0.2
        // 0.1 + 0.1 + 0.1 + 0.2 = 0.5
        EXPECT_NEAR(SpreadResolver::effective(in), 0.5f);
    });

    std::cout << "\n\033[1m" << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail ? 1 : 0;
}
