/**
 * test_PhiloxRng.cpp — Philox4x32-10 KAT + statistical + reversibility tests.
 * Compile: g++ -std=c++17 -I../src/dsp test_PhiloxRng.cpp -o test_px && ./test_px
 */
#include "PhiloxRng.hpp"
#include <iostream>
#include <sstream>
#include <cmath>
#include <vector>
#include <random>
#include <array>

using namespace redDot;
static int s_pass=0, s_fail=0;
#define SUITE(n) do{std::cout<<"\n["<<(n)<<"]\n";}while(0)
#define TEST(desc,...) do{ bool _ok=true; std::string _m; \
    try{__VA_ARGS__;}catch(const std::exception&_e){_ok=false;_m=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  PASS "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  FAIL "<<(desc); if(!_m.empty())std::cout<<" — "<<_m; std::cout<<"\n";} }while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ")");}while(0)
#define EXPECT_EQ(a,b) do{if((a)!=(b)){std::ostringstream s;s<<#a<<"="<<(a)<<" != "<<(b);throw std::runtime_error(s.str());}}while(0)
#define EXPECT_NEAR(a,b,e) do{if(std::fabs((a)-(b))>(e)){std::ostringstream s;s<<#a<<"="<<(a)<<" not~"<<(b);throw std::runtime_error(s.str());}}while(0)

static bool eq4(std::array<uint32_t,4> a, std::array<uint32_t,4> b){ return a==b; }

int main(){
    SUITE("Philox4x32-10 canonical known-answer vectors");
    TEST("zero counter/key", {
        EXPECT(eq4(philox4x32_10({0,0,0,0},{0,0}),
                   {0x6627e8d5,0xe169c58d,0xbc57ac4c,0x9b00dbd8}));
    });
    TEST("pi-digit counter/key", {
        EXPECT(eq4(philox4x32_10({0x243f6a88,0x85a308d3,0x13198a2e,0x03707344},
                                 {0xa4093822,0x299f31d0}),
                   {0xd16cfe09,0x94fdcceb,0x5001e420,0x24126ea1}));
    });

    SUITE("addressability / reversibility (phase-reverse property)");
    TEST("at(N) order-independent", {
        PhiloxRng a(12345), b(12345);
        for (uint64_t n : {0ull,1ull,7ull,100ull,815ull,4096ull,1ull,0ull})
            EXPECT_EQ(a.at(n), b.at(n));
    });
    TEST("816-draw block forward==reverse", {
        PhiloxRng r(0xBEEF);
        std::vector<float> fwd(816), rev(816);
        for (int i=0;i<816;++i)  fwd[i]=r.atUniform(i);
        for (int i=815;i>=0;--i) rev[i]=r.atUniform(i);
        for (int i=0;i<816;++i) EXPECT_EQ(fwd[i], rev[i]);
    });
    TEST("fillBlock reproducible + counter untouched", {
        PhiloxRng r(55); float a[64], b[64];
        r.fillBlock(1000,a,64); r.fillBlock(1000,b,64);
        for(int i=0;i<64;++i) EXPECT_EQ(a[i],b[i]);
        EXPECT_EQ(r.getCounter(),(uint64_t)0);
    });

    SUITE("uniform distribution");
    TEST("atUniform in [0,1)", {
        PhiloxRng u(99);
        for(int i=0;i<200000;++i){ float v=u.atUniform(i); EXPECT(v>=0.f && v<1.f); }
    });
    TEST("mean ~ 0.5", {
        PhiloxRng u(99); double acc=0; int N=300000;
        for(int i=0;i<N;++i) acc+=u.atUniform(i);
        EXPECT_NEAR(acc/N, 0.5, 0.01);
    });
    TEST("chi-square 10 buckets < 30", {
        PhiloxRng c(424242); int bk[10]={}; int M=1000000;
        for(int i=0;i<M;++i){int k=(int)(c.atUniform(i)*10); if(k<0)k=0; if(k>9)k=9; bk[k]++;}
        double chi=0,e=M/10.0; for(int i=0;i<10;++i){double d=bk[i]-e; chi+=d*d/e;}
        EXPECT(chi<30.0);
    });

    SUITE("facade & persistence");
    TEST("different seeds independent", {
        PhiloxRng k1(1),k2(2); int same=0;
        for(int i=0;i<1000;++i) if(k1.at(i)==k2.at(i)) same++;
        EXPECT(same<=2);
    });
    TEST("key+counter persistence round-trips", {
        PhiloxRng a(0xABCD); a.setCounter(500);
        PhiloxRng b; b.setKey(a.getKey()); b.setCounter(500);
        EXPECT_EQ(a.next(), b.next());
    });
    TEST("rewind reproduces sequence", {
        PhiloxRng r(7); float x0=r.nextFloat(),x1=r.nextFloat();
        r.rewind(); EXPECT_EQ(r.nextFloat(),x0); EXPECT_EQ(r.nextFloat(),x1);
    });

    SUITE("std::philox4x32 / philox4x64 conformance anchors");
    TEST("philox4x32 default-seeded 10000th invocation == 1955073260", {
        // std anchor: key={default_seed,0}, counter in X[0..1], outputs in order.
        auto stdval = [](uint64_t i)->uint32_t {
            std::array<uint32_t,2> key{20111115u,0u};
            uint64_t b=i/4; int slot=(int)(i%4);
            std::array<uint32_t,4> X{(uint32_t)(b&0xffffffff),(uint32_t)(b>>32),0u,0u};
            return philox4x32_10(X,key)[slot];
        };
        EXPECT_EQ(stdval(9999), 1955073260u);
    });
    TEST("philox4x64 default-seeded 10000th invocation == 3409172418970261260", {
        auto stdval = [](uint64_t i)->uint64_t {
            std::array<uint64_t,2> key{20111115ull,0ull};
            uint64_t b=i/4; int slot=(int)(i%4);
            std::array<uint64_t,4> X{b,0ull,0ull,0ull};
            return philox4x64_10(X,key)[slot];
        };
        EXPECT_EQ(stdval(9999), 3409172418970261260ull);
    });

    SUITE("Philox4x64Rng (64-bit engine)");
    TEST("addressable + reversible (816-draw)", {
        Philox4x64Rng r(0xBEEF);
        std::vector<double> fwd(816), rev(816);
        for(int i=0;i<816;++i) fwd[i]=r.atUniform(i);
        for(int i=815;i>=0;--i) rev[i]=r.atUniform(i);
        for(int i=0;i<816;++i) EXPECT_EQ(fwd[i],rev[i]);
    });
    TEST("uniform [0,1) + mean ~0.5", {
        Philox4x64Rng u(99); double acc=0; int N=300000;
        for(int i=0;i<N;++i){ double v=u.atUniform(i); EXPECT(v>=0.0&&v<1.0); acc+=v; }
        EXPECT_NEAR(acc/N, 0.5, 0.01);
    });
    TEST("works as std UniformRandomBitGenerator", {
        Philox4x64Rng g(123);
        std::uniform_int_distribution<int> d(0,99);
        int hist[100]={}; for(int i=0;i<100000;++i) hist[d(g)]++;
        int mn=1000000,mx=0; for(int i=0;i<100;++i){if(hist[i]<mn)mn=hist[i];if(hist[i]>mx)mx=hist[i];}
        EXPECT(mn>700 && mx<1300);
    });
    TEST("setCounter + key persistence round-trip", {
        Philox4x64Rng a(0xABCD); a.setCounter(500);
        Philox4x64Rng b; b.setKey(a.getKey()); b.setCounter(500);
        EXPECT_EQ(a.next(), b.next());
    });

    std::cout<<"\n"<<s_pass<<" passed, "<<s_fail<<" failed\n";
    return s_fail?1:0;
}
