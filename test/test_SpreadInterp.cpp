/**
 * test_SpreadInterp.cpp — spread interpolation invariants.
 * Compile: g++ -std=c++17 -I../src/dsp test_SpreadInterp.cpp -o test_si && ./test_si
 *
 * Guards the documented contract that a voice targeting its OWN draw (MONO_DRAW, or
 * AVERAGE_POLY with no other voices) is a NO-OP for the spread knob — for BOTH signs.
 * Regression: the spread<0 branch interpolated toward (1−target) and so shifted a
 * self-targeting draw by (1−2·original)·|amount|, audible as probability moving when
 * turning a lone Sands Mono spread knob negative.
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
    SUITE("self-target is a no-op for BOTH spread signs (the regression)");
    {
        float os[] = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};
        float spreads[] = {-1.0f, -0.9f, -0.4f, -0.001f, 0.001f, 0.4f, 0.9f, 1.0f};
        for (float o : os) {
            for (float sp : spreads) {
                // target == original → self-target → must be a no-op, both signs.
                TEST("interpolate(o, target=o, sp) == o", {
                    EXPECT_NEAR(SpreadInterp::interpolate(o, o, sp), o, 1e-6f);
                });
            }
        }
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
