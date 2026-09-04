/**
 * test_ScaleMaskArbiter.cpp — pure scale-mask arbitration + root transpose.
 *
 * Fills the runner's reserved `test_ScaleMaskArbiter` slot (was empty; on no branch). Covers the
 * header-only dotModular arbiter that ScaleManager shares verbatim, so what ships == what's tested:
 *   - resolveScaleMask: priority OVERRIDE > AUTHORED > FACTORY > all-12 (0xFFF), each &0xFFF.
 *   - rotateMask12:     circular 12-bit root transpose (TONIC_TRANSPOSE_BUILD_BRIEF).
 *   - normaliseToTonic: inverse of applying a root (round-trips).
 *
 * Header-only (no companion sources — see test/run_all.sh). No Rack SDK needed.
 */
#include "ScaleMaskArbiter.hpp"
#include <iostream>
#include <sstream>
#include <cstdint>

using dotModular::resolveScaleMask;
using dotModular::rotateMask12;
using dotModular::normaliseToTonic;

static int s_pass=0, s_fail=0;
#define SUITE(n) do{std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n";}while(0)
#define TEST(desc,...) do{ bool _ok=true; std::string _m; \
    try{__VA_ARGS__;}catch(const std::exception&_e){_ok=false;_m=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32mPASS\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31mFAIL\033[0m "<<(desc); if(!_m.empty())std::cout<<" -- "<<_m; std::cout<<"\n";} }while(0)
#define EXPECT_EQ(a__,b__) do{ if((a__)!=(b__)){ std::ostringstream _s; \
    _s<<std::hex<<#a__<<"=0x"<<(unsigned)(a__)<<" != "<<#b__<<"=0x"<<(unsigned)(b__); \
    throw std::runtime_error(_s.str()); } }while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ")");}while(0)

static constexpr uint16_t C_MAJOR = 0x0AB5;   // {0,2,4,5,7,9,11}
static constexpr uint16_t D_MAJOR = 0x0AD6;   // C major rotated up 2 -> {1,2,4,6,7,9,11}

int main(){
    SUITE("resolveScaleMask — priority OVERRIDE > AUTHORED > FACTORY > all-12");
    TEST("override wins over authored + factory", {
        EXPECT_EQ(resolveScaleMask(true,0x0111, true,0x0222, true,0x0444), 0x0111);
    });
    TEST("authored wins when override invalid", {
        EXPECT_EQ(resolveScaleMask(false,0x0111, true,0x0222, true,0x0444), 0x0222);
    });
    TEST("factory used when only factory valid", {
        EXPECT_EQ(resolveScaleMask(false,0x0111, false,0x0222, true,0x0444), 0x0444);
    });
    TEST("all-12 (0xFFF) when nothing valid", {
        EXPECT_EQ(resolveScaleMask(false,0x0111, false,0x0222, false,0x0444), 0x0FFF);
    });
    TEST("result is always masked to 12 bits (upper bits stripped)", {
        EXPECT_EQ(resolveScaleMask(true,0xFAB5, false,0,false,0), C_MAJOR);
    });

    SUITE("rotateMask12 — circular 12-bit root transpose");
    TEST("rotate by 0 is identity", { EXPECT_EQ(rotateMask12(C_MAJOR,0), C_MAJOR); });
    TEST("rotate by 12 is identity (mod 12)", { EXPECT_EQ(rotateMask12(C_MAJOR,12), C_MAJOR); });
    TEST("C major up 2 semitones == D major", { EXPECT_EQ(rotateMask12(C_MAJOR,2), D_MAJOR); });
    TEST("wraps circularly: bit 11 up 1 -> bit 0", {
        EXPECT_EQ(rotateMask12((uint16_t)(1u<<11),1), (uint16_t)0x001);
    });
    TEST("negative semis == +12 equivalent (-1 == +11)", {
        EXPECT_EQ(rotateMask12(C_MAJOR,-1), rotateMask12(C_MAJOR,11));
    });
    TEST("out-of-range semis reduced mod 12 (+14 == +2)", {
        EXPECT_EQ(rotateMask12(C_MAJOR,14), rotateMask12(C_MAJOR,2));
    });
    TEST("rotating up 12 in unit steps returns to start", {
        uint16_t m=C_MAJOR; for(int i=0;i<12;++i) m=rotateMask12(m,1);
        EXPECT_EQ(m, C_MAJOR);
    });

    SUITE("normaliseToTonic — inverse of applying a root");
    TEST("normalise(mask,0) is identity", { EXPECT_EQ(normaliseToTonic(C_MAJOR,0), C_MAJOR); });
    TEST("absolute D major normalised to tonic D == root-relative major pattern (C major shape)", {
        EXPECT_EQ(normaliseToTonic(D_MAJOR,2), C_MAJOR);
    });
    TEST("round-trip: apply root then normalise recovers the relative mask (all tonics)", {
        for(int tonic=0; tonic<12; ++tonic){
            uint16_t absolute = rotateMask12(C_MAJOR, tonic);      // relative -> absolute
            EXPECT_EQ(normaliseToTonic(absolute, tonic), C_MAJOR); // absolute -> relative
        }
    });

    std::cout<<"\n"<<s_pass<<" passed, "<<s_fail<<" failed\n";
    return s_fail ? 1 : 0;
}
