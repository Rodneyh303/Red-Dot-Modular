/**
 * test_SquaresRng.cpp
 * Compile: g++ -std=c++17 -I../src/dsp test_SquaresRng.cpp -o test_sq && ./test_sq
 * (or via the project's test build). No Rack dependency.
 */
#include "SquaresRng.hpp"
#include <iostream>
#include <sstream>
#include <cmath>
#include <vector>

using namespace redDot;

static int s_pass=0, s_fail=0;
#define SUITE(n) do{std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n";}while(0)
#define TEST(desc,...) do{ \
    bool _ok=true; std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31m✗\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" — "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e)       do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_EQ(a,b)  do{if((a)!=(b)){std::ostringstream s; \
    s<<#a<<"="<<(a)<<" != "<<#b<<"="<<(b);throw std::runtime_error(s.str());}}while(0)
#define EXPECT_NEAR(a,b,e) do{if(std::fabs((a)-(b))>(e)){std::ostringstream s; \
    s<<#a<<"="<<(a)<<" not near "<<#b<<"="<<(b);throw std::runtime_error(s.str());}}while(0)

int main(){
    SUITE("squares64 / addressability");
    TEST("at(N) is a pure function of (N,key) — order independent", {
        SquaresRng a(12345), b(12345);
        for (uint64_t n : {0ULL,1ULL,7ULL,100ULL,815ULL,1ULL,0ULL})
            EXPECT_EQ(a.at(n), b.at(n));
    });
    TEST("at(N) independent of any sequential calls between", {
        SquaresRng r(777);
        uint64_t v100 = r.at(100);
        for (int i=0;i<50;++i) (void)r.next();   // churn the counter
        EXPECT_EQ(r.at(100), v100);              // unchanged
    });

    SUITE("reversibility (the phase-reverse property)");
    TEST("816-draw block drawn in reverse matches forward draw", {
        SquaresRng r(0xBEEF);
        std::vector<float> fwd(816), rev(816);
        for (int i=0;i<816;++i)  fwd[i] = r.atUniformF(i);
        for (int i=815;i>=0;--i) rev[i] = r.atUniformF(i);
        for (int i=0;i<816;++i) EXPECT_EQ(fwd[i], rev[i]);
    });
    TEST("fillBlock reproduces the same block forwards/again", {
        SquaresRng r(55);
        float a[64], b[64];
        r.fillBlock(1000, a, 64);
        r.fillBlock(1000, b, 64);
        for (int i=0;i<64;++i) EXPECT_EQ(a[i], b[i]);
    });

    SUITE("uniform distribution");
    TEST("atUniform stays in [0,1)", {
        SquaresRng u(99);
        for (int i=0;i<200000;++i){ double v=u.atUniform(i); EXPECT(v>=0.0 && v<1.0); }
    });
    TEST("mean ~ 0.5 over 200k", {
        SquaresRng u(99); double sum=0; int N=200000;
        for (int i=0;i<N;++i) sum+=u.atUniform(i);
        EXPECT_NEAR(sum/N, 0.5, 0.01);
    });
    TEST("chi-square over 10 buckets is sane (<30, 9 dof)", {
        SquaresRng c(424242); int bk[10]={}; int M=1000000;
        for (int i=0;i<M;++i){ int k=(int)(c.atUniform(i)*10); if(k<0)k=0; if(k>9)k=9; bk[k]++; }
        double chi=0, e=M/10.0;
        for (int i=0;i<10;++i){ double d=bk[i]-e; chi+=d*d/e; }
        EXPECT(chi < 30.0);
    });

    SUITE("stream identity & facade");
    TEST("different seeds → independent streams", {
        SquaresRng k1(1), k2(2); int same=0;
        for (int i=0;i<1000;++i) if (k1.at(i)==k2.at(i)) same++;
        EXPECT(same <= 2);
    });
    TEST("seed(s1,s2) two-word form + rewind reproduces", {
        SquaresRng rng; rng.seed(111,222);
        float x0=rng.nextFloat(), x1=rng.nextFloat();
        rng.rewind();
        EXPECT_EQ(rng.nextFloat(), x0);
        EXPECT_EQ(rng.nextFloat(), x1);
    });
    TEST("drawBlock advances counter; fillBlock does not", {
        SquaresRng d(7); float blk[10];
        d.drawBlock(blk,10);
        EXPECT_EQ(d.getCounter(), (uint64_t)10);
        float blk2[10]; d.fillBlock(0,blk2,10);
        EXPECT_EQ(d.getCounter(), (uint64_t)10);
    });
    TEST("next() equals at(counter) then advances", {
        SquaresRng rng(31337);
        uint64_t at0 = rng.at(0);
        EXPECT_EQ(rng.next(), at0);
        EXPECT_EQ(rng.getCounter(), (uint64_t)1);
    });
    TEST("setKey/getCounter round-trips for persistence", {
        SquaresRng rng(0xABCD);
        uint64_t k = rng.getKey();
        rng.setCounter(500);
        SquaresRng s2; s2.setKey(k); s2.setCounter(500);
        EXPECT_EQ(rng.next(), s2.next());   // same key+counter → same stream
    });

    std::cout << "\n" << (s_fail? "\033[31m":"\033[32m")
              << s_pass << " passed, " << s_fail << " failed\033[0m\n";
    return s_fail ? 1 : 0;
}
