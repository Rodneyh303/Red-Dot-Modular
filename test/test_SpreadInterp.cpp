/**
 * test_SpreadInterp.cpp — spread interpolation invariants.
 * Compile: g++ -std=c++17 -I../src/dsp test_SpreadInterp.cpp -o test_si && ./test_si
 *
 * Guards the spread interpolation contract for a voice targeting its OWN draw
 * (voice-1 target):
 *   • spread >= 0 → NO-OP (nothing to converge toward).
 *   • spread <  0 → inverts toward (1−target) = (1−original); V1 negative spread is
 *     intended and meaningful (e.g. Sands Mono voice-1-target mode).
 * (Earlier the contract was 'no-op for both signs'; the negative-inverts behaviour is
 * now the agreed design, so this suite asserts the asymmetric rule.)
 */
#include "test_stubs.hpp"   // rack:: random/dsp stubs (PatternEngine deps)
#include "SpreadInterp.hpp" // pulls the test-local rack.hpp shim for math::clamp
#include <iostream>
#include <sstream>
#include <cmath>

using namespace redDot;
static int s_pass=0, s_fail=0;
#define SUITE(n) do{std::cout<<"\n["<<(n)<<"]\n";}while(0)
#define TEST(desc,...) do{ bool _ok=true; std::string _m; \
    try{__VA_ARGS__;}catch(const std::exception&_e){_ok=false;_m=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  PASS "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  FAIL "<<(desc); if(!_m.empty())std::cout<<" — "<<_m; std::cout<<"\n";} }while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ")");}while(0)
#define EXPECT_NEAR(a,b,e) do{if(std::fabs((a)-(b))>(e)){std::ostringstream s;s<<#a<<"="<<(a)<<" not~"<<(b);throw std::runtime_error(s.str());}}while(0)

int main(){
    SUITE("self-target: POSITIVE/zero no-op, NEGATIVE inverts toward (1-original)");
    {
        // Agreed contract (supersedes the older 'both signs no-op'): a lane targeting
        // its OWN draw (voice-1 target) does NOTHING for
        // spread >= 0 (nothing to converge toward), but spread < 0 inverts the draw toward
        // (1 - original) — V1 negative spread is meaningful and intended.
        float os[] = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};
        float posSpreads[] = {0.0f, 0.001f, 0.4f, 0.9f, 1.0f};
        for (float o : os)
            for (float sp : posSpreads)
                TEST("self-target, spread>=0 → no-op", {
                    EXPECT_NEAR(SpreadInterp::interpolate(o, o, sp), o, 1e-6f);
                });
        // Negative self-target inverts: o + ((1-o)-o)*|s| = o + (1-2o)*|s|.
        float negSpreads[] = {-0.001f, -0.4f, -0.9f, -1.0f};
        for (float o : os)
            for (float sp : negSpreads)
                TEST("self-target, spread<0 → toward (1-o)", {
                    float expected = o + (1.0f - 2.0f*o) * std::fabs(sp);
                    EXPECT_NEAR(SpreadInterp::interpolate(o, o, sp), expected, 1e-6f);
                });
        // Concrete: o=0.8, s=-1 → inverts to 0.2; s=+1 → stays 0.8.
        TEST("o=0.8 s=-1 → 0.2 (invert)", { EXPECT_NEAR(SpreadInterp::interpolate(0.8f,0.8f,-1.0f), 0.2f, 1e-6f); });
        TEST("o=0.8 s=+1 → 0.8 (no-op)",  { EXPECT_NEAR(SpreadInterp::interpolate(0.8f,0.8f, 1.0f), 0.8f, 1e-6f); });
    }

    SUITE("zero spread is always a no-op");
    TEST("interpolate(0.3, 0.8, 0) == 0.3", { EXPECT_NEAR(SpreadInterp::interpolate(0.3f,0.8f,0.f), 0.3f, 1e-6f); });

    SUITE("cross-target spread still works (fix must not disturb real spreading)");
    // spread>0 toward target: 0.3 + (0.8-0.3)*0.5 = 0.55
    TEST("pos toward target", { EXPECT_NEAR(SpreadInterp::interpolate(0.3f,0.8f,0.5f), 0.55f, 1e-6f); });
    // spread<0 toward (1-target): 0.3 + ((1-0.8)-0.3)*0.9 = 0.3 + (-0.1)*0.9 = 0.21
    TEST("neg toward (1-target)", { EXPECT_NEAR(SpreadInterp::interpolate(0.3f,0.8f,-0.9f), 0.21f, 1e-6f); });
    // full positive spread reaches target
    TEST("s=+1 reaches target", { EXPECT_NEAR(SpreadInterp::interpolate(0.2f,0.9f,1.0f), 0.9f, 1e-6f); });

    SUITE("result stays clamped to [0,1]");
    TEST("no overshoot high", { float r=SpreadInterp::interpolate(0.9f,0.05f,-1.0f); EXPECT(r>=0.f && r<=1.f); });
    TEST("no overshoot low",  { float r=SpreadInterp::interpolate(0.1f,0.95f,-1.0f); EXPECT(r>=0.f && r<=1.f); });

    std::cout<<"\n"<<s_pass<<" passed, "<<s_fail<<" failed\n";
    return s_fail ? 1 : 0;
}
