/**
 * test_qmix_rng.cpp — the q-mix (source-select) Philox stream.
 * Verifies the RNG generation added for the q-mix lane: an INDEPENDENT, DETERMINISTIC,
 * ADDRESSABLE/REVERSIBLE stream keyed by STREAM_SOURCE_SELECT, exactly as PatternEngine
 * seeds/draws it (seed64(deriveKey(seedFloat, STREAM_SOURCE_SELECT)); atUniform(pos*DRAW_CHUNK+cursor)).
 * Compile: g++ -std=c++17 -I../src/dsp test_qmix_rng.cpp -o test_qmix && ./test_qmix
 */
#include "PhiloxRng.hpp"
#include <iostream>
#include <sstream>
#include <cmath>
#include <array>

using namespace redDot;
static int s_pass=0, s_fail=0;
#define SUITE(n) do{std::cout<<"\n["<<(n)<<"]\n";}while(0)
#define TEST(desc,...) do{ bool _ok=true; std::string _m; \
    try{__VA_ARGS__;}catch(const std::exception&_e){_ok=false;_m=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  PASS "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  FAIL "<<(desc); if(!_m.empty())std::cout<<" — "<<_m; std::cout<<"\n";} }while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ")");}while(0)

// Mirror of PatternEngine's draw addressing (DRAW_CHUNK=1024; pos in ctr, cursor intra-draw).
static constexpr uint64_t DRAW_CHUNK = 1024;
static PhiloxRng seedStream(float seedFloat, uint64_t stream) {
    PhiloxRng r; r.seed64(seed::deriveKey(seedFloat, stream)); return r;
}
static float drawAt(const PhiloxRng& r, int64_t pos, uint64_t cursor) {
    return r.atUniform((uint64_t)pos * DRAW_CHUNK + cursor);
}

int main(){
    const float SEED = 4.2f;

    SUITE("stream reserved + distinct key from rhythm/melody/CA");
    TEST("STREAM_SOURCE_SELECT == 3 (the reserved slot)", {
        EXPECT(seed::STREAM_SOURCE_SELECT == 3);
    });
    TEST("deriveKey(SOURCE_SELECT) != deriveKey(RHYTHM/MELODY/CA) for same seed float", {
        uint64_t ks = seed::deriveKey(SEED, seed::STREAM_SOURCE_SELECT);
        EXPECT(ks != seed::deriveKey(SEED, seed::STREAM_RHYTHM));
        EXPECT(ks != seed::deriveKey(SEED, seed::STREAM_MELODY));
        EXPECT(ks != seed::deriveKey(SEED, seed::STREAM_CA));
    });

    SUITE("independence — q-mix decorrelates from melody (\"which notes\" vs \"where\")");
    TEST("source-select sequence differs from the melody sequence (same seed float)", {
        PhiloxRng ss = seedStream(SEED, seed::STREAM_SOURCE_SELECT);
        PhiloxRng mel= seedStream(SEED, seed::STREAM_MELODY);
        int diff=0; const int N=816;
        for(int i=0;i<N;++i) if (ss.at((uint64_t)i) != mel.at((uint64_t)i)) ++diff;
        // Independent 32-bit streams: collisions ~N/2^32 ≈ 0, so essentially all differ.
        EXPECT(diff > N-2);
    });
    TEST("uniform draws land in [0,1) on the source-select stream", {
        PhiloxRng ss = seedStream(SEED, seed::STREAM_SOURCE_SELECT);
        double acc=0; const int N=200000;
        for(int i=0;i<N;++i){ float v=ss.atUniform((uint64_t)i); EXPECT(v>=0.f && v<1.f); acc+=v; }
        EXPECT(std::fabs(acc/N - 0.5) < 0.01);   // mean ≈ 0.5
    });

    SUITE("determinism — same seed float reproduces the stream");
    TEST("re-seeding with the same float gives identical draws", {
        PhiloxRng a = seedStream(SEED, seed::STREAM_SOURCE_SELECT);
        PhiloxRng b = seedStream(SEED, seed::STREAM_SOURCE_SELECT);
        for(int pos=0;pos<32;++pos) for(uint64_t c=0;c<8;++c)
            EXPECT(drawAt(a,pos,c) == drawAt(b,pos,c));
    });
    TEST("different seed floats give a different stream", {
        PhiloxRng a = seedStream(1.0f, seed::STREAM_SOURCE_SELECT);
        PhiloxRng b = seedStream(9.0f, seed::STREAM_SOURCE_SELECT);
        int diff=0; for(int i=0;i<256;++i) if (a.at((uint64_t)i)!=b.at((uint64_t)i)) ++diff;
        EXPECT(diff > 254);
    });

    SUITE("addressable / reversible — the Mode-E reverse/jump foundation");
    TEST("forward then backward re-derive identical draws (per-position replay)", {
        PhiloxRng ss = seedStream(SEED, seed::STREAM_SOURCE_SELECT);
        const int N=816; float fwd[816], rev[816];
        for(int i=0;i<N;++i)   fwd[i]=drawAt(ss,i,0);
        for(int i=N-1;i>=0;--i) rev[i]=drawAt(ss,i,0);   // scrub backward
        for(int i=0;i<N;++i) EXPECT(fwd[i]==rev[i]);
    });
    TEST("at(N-1) re-derives the previous draw exactly (no stored history)", {
        PhiloxRng ss = seedStream(SEED, seed::STREAM_SOURCE_SELECT);
        for(int64_t pos=1;pos<64;++pos)
            EXPECT(drawAt(ss,pos-1,0) == drawAt(ss,pos-1,0) && drawAt(ss,pos,0)!=drawAt(ss,pos-1,0));
    });

    std::cout<<"\n"<<s_pass<<" passed, "<<s_fail<<" failed\n";
    return s_fail ? 1 : 0;
}
