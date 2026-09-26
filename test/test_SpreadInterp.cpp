/**
 * test_SpreadInterp.cpp — spread interpolation invariants (Phase 3: copula mix2).
 *
 * Guards the spread contract after the copula rework:
 *   • spread == 0  → returns original exactly (bit-identity).
 *   • self-target (own == leader) + spread > 0 → no-op (V1 is already the target).
 *   • self-target + spread = -1 → 1 - original (full invert).
 *   • spread = +1 → returns targetValue (full adherence, mix2 special case).
 *   • spread = -1 → returns 1 - targetValue (mirror, mix2 special case — the old 1-p special
 *     case is gone, mix2 handles it).
 *   • result always in [0, 1] (uniform marginal preserved — the whole point of the rework).
 *   • deterministic (same inputs → same output).
 */
#include "test_stubs.hpp"
#include "SpreadInterp.hpp"
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
    SUITE("spread == 0 → bit-identity");
    {
        float vals[] = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};
        for (float o : vals)
            for (float t : vals)
                TEST("zero spread returns original", {
                    EXPECT_NEAR(SpreadInterp::interpolate(o, t, 0.0f), o, 0.0f);  // EXACT
                });
    }

    SUITE("self-target (own == leader)");
    {
        float os[] = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};
        float posSpreads[] = {0.001f, 0.4f, 0.9f, 1.0f};
        for (float o : os)
            for (float sp : posSpreads)
                TEST("self-target, spread>0 → no-op", {
                    EXPECT_NEAR(SpreadInterp::interpolate(o, o, sp), o, 1e-6f);
                });
        // Self-target at rho=-1 → full invert (1 - original).
        TEST("self-target s=-1 → 1-original", {
            EXPECT_NEAR(SpreadInterp::interpolate(0.8f, 0.8f, -1.0f), 0.2f, 1e-6f);
        });
        TEST("self-target s=-1 (0.3) → 0.7", {
            EXPECT_NEAR(SpreadInterp::interpolate(0.3f, 0.3f, -1.0f), 0.7f, 1e-6f);
        });
    }

    SUITE("endpoints: rho = +1 and -1");
    {
        TEST("rho=+1 reaches target", {
            EXPECT_NEAR(SpreadInterp::interpolate(0.2f, 0.9f, 1.0f), 0.9f, 1e-6f);
        });
        TEST("rho=-1 reaches 1-target (mirror)", {
            EXPECT_NEAR(SpreadInterp::interpolate(0.3f, 0.8f, -1.0f), 0.2f, 1e-6f);
        });
        TEST("rho=+1 reaches target (0.5,0.5)", {
            EXPECT_NEAR(SpreadInterp::interpolate(0.5f, 0.5f, 1.0f), 0.5f, 1e-6f);
        });
    }

    SUITE("mid-rho: uniform marginal + determinism");
    {
        float os[] = {0.05f, 0.2f, 0.5f, 0.8f, 0.95f};
        float ts[] = {0.1f, 0.4f, 0.6f, 0.9f};
        float rhos[] = {-0.8f, -0.4f, 0.3f, 0.7f, 0.95f};
        for (float o : os)
            for (float t : ts)
                for (float r : rhos) {
                    TEST("result in [0,1]", {
                        float v = SpreadInterp::interpolate(o, t, r);
                        EXPECT(v >= 0.0f && v <= 1.0f);
                    });
                    TEST("deterministic", {
                        float a = SpreadInterp::interpolate(o, t, r);
                        float b = SpreadInterp::interpolate(o, t, r);
                        EXPECT(a == b);
                    });
                }
    }

    std::cout<<"\n"<<s_pass<<" passed, "<<s_fail<<" failed\n";
    return s_fail ? 1 : 0;
}
