/**
 * test_quantize_engine.cpp — the REAL SequencerEngine quantiser (Mode C/D/F core).
 *
 * Fills the runner's reserved `test_quantize_engine` slot (was empty). Drives the actual
 * SequencerEngine::quantize() / degreeOf() — NOT a mirror — so it covers the non-12
 * `degreeVolts` path that test_modes_bcd's 12-TET mirror cannot reach (the gap flagged in
 * MODES_C_D_QUANTIZER_PRERELEASE). Verifies: nearest-ACTIVE-degree snapping (mask-respecting),
 * register preservation, empty-scale passthrough, and microtonal snapping to real tuning degrees.
 *
 * Compile: see test/run_all.sh (companions: SequencerEngine.cpp GateState.cpp PatternEngine.cpp).
 */
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "SequencerEngine.hpp"
#include <iostream>
#include <sstream>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>

// ── framework (repo convention) ──────────────────────────────────────────────
static int s_pass=0, s_fail=0;
#define SUITE(n) do{std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n";}while(0)
#define TEST(desc,...) do{ bool _ok=true; std::string _m; \
    try{__VA_ARGS__;}catch(const std::exception&_e){_ok=false;_m=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32mPASS\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31mFAIL\033[0m "<<(desc); if(!_m.empty())std::cout<<" -- "<<_m; std::cout<<"\n";} }while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ")");}while(0)
#define EXPECT_NEAR(a__,b__,eps__) do{ if(std::fabs((a__)-(b__))>(eps__)){ std::ostringstream _s; \
    _s<<#a__<<"="<<(a__)<<" not near "<<#b__<<"="<<(b__); throw std::runtime_error(_s.str()); } }while(0)

// ── helpers ──────────────────────────────────────────────────────────────────
static void setScale(SequencerEngine& e, std::initializer_list<int> degs, float w=1.f){
    e.activeSemiCount=0;
    for(int d: degs){ e.activeSemiList[e.activeSemiCount]=d; e.activeSemiWeight[e.activeSemiCount]=w; ++e.activeSemiCount; }
    e.lastQuantIn=-100.f;                 // quantize() caches on input; bust it after a scale change
}
static float q(SequencerEngine& e, float v){ e.lastQuantIn=-100.f; return e.quantize(v); }

int main(){
    SequencerEngine eng;

    SUITE("12-TET: snaps to nearest ACTIVE degree, mask-respecting, register-preserving");
    setScale(eng, {0,2,4,5,7,9,11});      // C major
    TEST("exact active degree returned unchanged", {
        EXPECT_NEAR(q(eng, 4.f/12.f), 4.f/12.f, 1e-4f);          // E stays E
    });
    TEST("input just off an active degree snaps to it", {
        EXPECT_NEAR(q(eng, 4.f/12.f + 0.02f), 4.f/12.f, 1e-4f);
    });
    TEST("input near an INACTIVE semitone (C#) snaps to nearest ACTIVE, never C#", {
        float out = q(eng, 1.f/12.f + 0.005f);                   // just above C# (not in scale)
        bool isC = std::fabs(out-0.f)      < 1e-3f;
        bool isD = std::fabs(out-2.f/12.f) < 1e-3f;
        EXPECT(isC || isD);                                      // landed on an in-scale degree
        EXPECT(std::fabs(out - 1.f/12.f) > 1e-3f);               // NOT the masked-out C#
    });
    TEST("register preserved: an octave-3 input quantises within octave 3", {
        EXPECT_NEAR(q(eng, 3.f + 4.f/12.f + 0.01f), 3.f + 4.f/12.f, 1e-3f);
    });

    SUITE("empty scale is a passthrough");
    TEST("activeSemiCount==0 -> input returned (clamped 0..5)", {
        eng.activeSemiCount=0; eng.lastQuantIn=-100.f;
        EXPECT_NEAR(eng.quantize(1.234f), 1.234f, 1e-4f);
    });

    SUITE("MICROTONAL: snaps to real tuning degrees via degreeVolts (the 12-TET-mirror gap)");
    {
        eng.pe.tuning.resetToEqual12TET();
        eng.pe.tuning.cents[2] = 250.f;                          // degree 2 -> 250c (neutral, non-12-TET)
        eng.pe.tuning.recomputeDefaultFlag();
        TEST("tuning now flagged non-default (activates the cents path)", {
            EXPECT(!eng.pe.tuning.isDefault12TET);
        });
        TEST("degreeOf() names degree 2 at 250c (nearestDegree, not round(frac*12))", {
            EXPECT(eng.degreeOf(250.f/1200.f) == 2);
        });
        setScale(eng, {0,2,4,5,7,9,11});
        TEST("quantize snaps to 250c (0.2083V), NOT the 12-TET slot 2/12 or 3/12", {
            float target = 250.f/1200.f;                         // 0.20833V
            float out = q(eng, target - 0.005f);
            EXPECT_NEAR(out, target, 2e-3f);                     // the ACTUAL tuning degree
            EXPECT(std::fabs(out - 2.f/12.f) > 5e-3f);           // not the old semitone slot
            EXPECT(std::fabs(out - 3.f/12.f) > 5e-3f);
        });
        TEST("degrees left at i*100 still land on their 12-TET voltage", {
            EXPECT_NEAR(q(eng, 4.f/12.f + 0.01f), 4.f/12.f, 1e-3f);   // degree 4 unchanged (400c)
        });
    }

    std::cout<<"\n"<<s_pass<<" passed, "<<s_fail<<" failed\n";
    return s_fail ? 1 : 0;
}
