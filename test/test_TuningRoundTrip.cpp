// test_TuningRoundTrip.cpp — regression guard for the microtonal roadmap (Colonnades Duo / Phase 3).
//
// TWO guarantees this test locks down BEFORE the engine N-widening (issue A) begins, so the widen
// can't silently break them:
//
//   1. ScalaFile write→parse ROUND-TRIP is lossless (cents preserved), at N=12 AND N=24. This is the
//      Save→Load path a Micro exposes; the Duo extends it to 24 degrees. If writeScala/parseScala
//      drift, load-after-save would corrupt a user's tuning.
//   2. TuningTable behaves correctly at N=24 (the Micro-24 degree count): degreeVolts maps each degree
//      to cents/1200 for ANY N, isDefault12TET stays FALSE at N!=12, and the equal-division property
//      that makes N=12 byte-identical is unaffected by exercising N=24 (the load-bearing invariant the
//      whole widen must preserve).
//
// Pure C++ / no Rack (ScalaFile + TuningTable are header-only, stdlib-only), so it builds standalone
// and runs in run_all.sh. NOTE: the .dmtune (TuningPreset) round-trip is NOT covered here because
// TuningPreset.hpp pulls in <rack.hpp> (jansson json_t) — it can only be exercised in the plugin tree,
// not the header-only harness. Its logic mirrors readArr/saveTuningPreset symmetrically; guarded by
// Rack-load verification instead.
//
// Build (standalone):
//   g++ -std=c++17 -Isrc test/test_TuningRoundTrip.cpp -o /tmp/ttr && /tmp/ttr

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "tuning/ScalaFile.hpp"
#include "tuning/TuningTable.hpp"

using dotModular::ScalaFile;
using dotModular::parseScala;
using dotModular::writeScala;
using dotModular::TuningTable;

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)
#define EXPECT_EQ(a,b) do { if((a)!=(b)) { std::ostringstream _s; \
    _s << "EXPECT_EQ(" #a "," #b ") : " << (long long)(a) << " != " << (long long)(b); \
    throw std::runtime_error(_s.str()); } } while(0)
#define EXPECT_NEAR(a,b,eps) do { double _d = std::fabs((double)(a)-(double)(b)); \
    if(_d > (eps)) { std::ostringstream _s; \
    _s << "EXPECT_NEAR(" #a "," #b ") : " << (double)(a) << " vs " << (double)(b) << " (|d|=" << _d << ")"; \
    throw std::runtime_error(_s.str()); } } while(0)
#define EXPECT_BITEQ(a,b) do { float _x=(a),_y=(b); if(std::memcmp(&_x,&_y,sizeof(float))!=0) { \
    std::ostringstream _s; _s << "EXPECT_BITEQ(" #a "," #b ") : " << _x << " != " << _y; \
    throw std::runtime_error(_s.str()); } } while(0)

#include <cstring>
static int g_pass = 0, g_fail = 0;

// Round-trip a cents list through writeScala → parseScala and assert the degree count + values survive.
static void roundTrip(const std::vector<float>& cents, const char* desc) {
    std::string text = writeScala(cents, desc);
    ScalaFile sf = parseScala(text);
    if (!sf.ok()) throw std::runtime_error(std::string("parse failed: ") + sf.errorMessage);
    if (sf.degreeCount() != (int)cents.size())
        throw std::runtime_error("degree count changed: wrote " + std::to_string(cents.size()) +
                                 " got " + std::to_string(sf.degreeCount()));
    for (size_t i = 0; i < cents.size(); ++i) {
        double d = std::fabs((double)sf.centsFromRoot[i] - (double)cents[i]);
        if (d > 1e-3) throw std::runtime_error("cents[" + std::to_string(i) + "] drifted by " +
                                               std::to_string(d));
    }
}

int main() {
    SUITE("ScalaFile write→parse round-trip (lossless cents)");
    TEST("12-tone equal temperament (11 steps + 1200 period) survives round-trip", {
        std::vector<float> c;
        for (int i = 1; i <= 11; ++i) c.push_back((float)i * 100.f);
        c.push_back(1200.f);                       // period (octave), Scala convention
        roundTrip(c, "12-TET");
    });
    TEST("24-EDO (23 quarter-tone steps + 1200 period) survives round-trip", {
        std::vector<float> c;
        for (int i = 1; i <= 23; ++i) c.push_back((float)i * 50.f);   // 24-EDO step = 50 cents
        c.push_back(1200.f);
        roundTrip(c, "24-EDO");
    });
    TEST("irrational cents (just-intonation-ish) preserved to 3 decimals", {
        std::vector<float> c = {111.731f, 203.910f, 315.641f, 386.314f, 498.045f,
                                590.224f, 701.955f, 813.686f, 884.359f, 996.090f,
                                1088.269f, 1200.f};
        roundTrip(c, "5-limit JI");
    });
    TEST("7-note diatonic (6 steps + octave) round-trips as 7 entries", {
        std::vector<float> c = {200.f, 400.f, 500.f, 700.f, 900.f, 1100.f, 1200.f};
        roundTrip(c, "major");
    });

    SUITE("TuningTable at N=24 (Micro-24 degree count)");
    TEST("degreeVolts(i) == cents[i]/1200 for a 24-EDO table, all 24 degrees", {
        TuningTable tt;
        tt.N = 24;
        for (int i = 0; i < 24; ++i) tt.cents[i] = (float)i * 50.f;   // 24-EDO
        tt.recomputeDefaultFlag();
        EXPECT(!tt.isDefault12TET);                 // N!=12 ⇒ never the byte-identical fast-path
        // Mirror degreeVolts' EXACT expression (cents * (1/1200) reciprocal-multiply) so the compare
        // is bit-exact against its own contract — NOT a /1200 division (differs in the last bit).
        for (int i = 0; i < 24; ++i)
            EXPECT_BITEQ(tt.degreeVolts(i), ((float)i * 50.f) * (1.f / 1200.f));
    });
    TEST("degree 12 of 24-EDO is exactly the tritone (600 cents = 0.5V)", {
        TuningTable tt;
        tt.N = 24;
        for (int i = 0; i < 24; ++i) tt.cents[i] = (float)i * 50.f;
        EXPECT_BITEQ(tt.degreeVolts(12), 0.5f);
    });
    TEST("degreeVolts clamps out-of-range degree indices into [0, MAXN-1]", {
        TuningTable tt;
        tt.N = 24;
        for (int i = 0; i < 24; ++i) tt.cents[i] = (float)i * 50.f;
        EXPECT_BITEQ(tt.degreeVolts(-3), tt.degreeVolts(0));         // clamp low
        EXPECT_BITEQ(tt.degreeVolts(999), tt.degreeVolts(TuningTable::MAXN - 1)); // clamp high
    });
    TEST("MAXN has room for 24 degrees", {
        EXPECT(TuningTable::MAXN >= 24);
    });

    SUITE("nearestDegree (Mode C/D degree naming under a custom tuning)");
    TEST("24-EDO: nearestDegree maps each degree's own volt back to that degree", {
        TuningTable tt;
        tt.N = 24;
        for (int i = 0; i < 24; ++i) tt.cents[i] = (float)i * 50.f;
        for (int i = 0; i < 24; ++i)
            EXPECT_EQ(tt.nearestDegree(tt.degreeVolts(i)), i);
    });
    TEST("24-EDO: a frac between degrees snaps to the closer one", {
        TuningTable tt;
        tt.N = 24;
        for (int i = 0; i < 24; ++i) tt.cents[i] = (float)i * 50.f;
        // Midpoint between degree 0 (0¢) and degree 1 (50¢) is 25¢.
        EXPECT_EQ(tt.nearestDegree(20.f / 1200.f), 0);   // below midpoint → degree 0
        EXPECT_EQ(tt.nearestDegree(30.f / 1200.f), 1);   // above midpoint → degree 1
    });
    TEST("7-note scale: nearestDegree only ever returns 0..N-1 (respects N, not MAXN)", {
        TuningTable tt;
        tt.N = 7;
        const float maj[7] = {0.f, 200.f, 400.f, 500.f, 700.f, 900.f, 1100.f};
        for (int i = 0; i < 7; ++i) tt.cents[i] = maj[i];
        for (float f = 0.f; f < 1.f; f += 0.013f) {
            int d = tt.nearestDegree(f);
            EXPECT(d >= 0 && d < 7);
        }
    });

    SUITE("N=12 invariant unaffected by exercising N=24");
    TEST("resetToEqual12TET after an N=24 detour restores byte-identical 12-TET", {
        TuningTable tt;
        tt.N = 24; for (int i = 0; i < 24; ++i) tt.cents[i] = (float)i * 50.f;
        tt.recomputeDefaultFlag(); EXPECT(!tt.isDefault12TET);
        tt.resetToEqual12TET();
        EXPECT(tt.N == 12);
        EXPECT(tt.isDefault12TET);
        // Same reciprocal-multiply form as degreeVolts (cents[i]=i*100 at the equal-division default).
        for (int i = 0; i < 12; ++i)
            EXPECT_BITEQ(tt.degreeVolts(i), ((float)i * 100.f) * (1.f / 1200.f));
    });

    std::cout << "\n-----\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
