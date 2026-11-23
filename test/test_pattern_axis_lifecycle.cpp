#include "test_stubs.hpp"
/**
 * test_pattern_axis_lifecycle.cpp
 * CA2 — the per-axis pattern-engine lifecycle (rhythm / melody / qmix) is
 * hand-unrolled x3. This PINS the three lifecycles equivalent (the audit's lighter
 * option) so a cadence/cache change to one axis that does NOT match the others is
 * caught, instead of becoming a silent per-axis divergence.
 *
 * WHAT CA2 IS. PatternEngine writes the same lifecycle out three times: a Philox
 * stream, a signed draw counter, an intra-draw cursor, advance/zero/begin, a draw
 * cache + its prune, a keyed/full reseed, and the draw-addressing base
 * `ctr*DRAW_CHUNK + cursor`. The CONTENT differs per axis (rhythm = gate fields,
 * melody = pitch fields, qmix = the mix field); the LIFECYCLE is identical. The
 * bounded-cache fix had to touch all three — a one-axis miss would have been a
 * silent divergence. `pruneDrawCache_` is ALREADY one template, proving the shape
 * parameterises; the rest is not, so this test is the pin until (if) it is.
 *
 * WHAT THIS IS NOT. Not a values test — the three streams are DISTINCT BY DESIGN
 * (different STREAM_* keys), so their draw VALUES differ and must. This pins the
 * BOOKKEEPING in lockstep and, separately, pins that the streams stay distinct
 * (a drift that actually happened once — see G6).
 *
 * METHOD. One op script, run through each axis via a hooks table (the only
 * per-axis thing is which members/methods the hooks name). The axes are asserted
 * in lockstep after every op. Because the driver is shared, the test cannot itself
 * encode a per-axis asymmetry — an asymmetry can only come from the engine.
 *
 * Compile: see test/run_all.sh (the registry is the source of truth).
 */
#include "PatternEngine.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <functional>
#include <vector>
#include <array>

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

using PE = PatternEngine;

// A per-axis hook table. The ONLY per-axis content is which members/methods each
// hook names; everything the test DOES with them is axis-blind.
struct Axis {
    const char* name;
    std::function<void(PE&,int)>       advance;     // advanceXDraw(dir)
    std::function<void(PE&)>           zero;        // zeroXIndex()
    std::function<void(PE&)>           begin;       // beginXDraw()  — cursor := 0
    std::function<void(PE&,float)>     seedKeyed;   // seedXPhilox(seedFloat)
    std::function<void(PE&)>           seedFull;    // seedXPhiloxFull()
    std::function<float(PE&)>          philox;      // philoxX() — returns uniform, bumps cursor
    std::function<int64_t(const PE&)>  ctr;         // read XDrawCtr
    std::function<uint64_t(const PE&)> cursor;      // read XCursor
    std::function<size_t(const PE&)>   cacheSize;   // XDrawCache_.size()
    std::function<void(PE&,int64_t)>   touchCache;  // cachedXDraw(pos) — populate one key
    std::function<void(PE&,int64_t)>   patternAt0;  // patternXAt(pos, slew=0) — prunes, r==0 raw shortcut
    std::function<bool(PE&,int64_t)>   shortcutIsRaw; // patternXAt(pos,0)==cachedXDraw(pos).draw bitwise
};

template<class Draw>
static bool sameBits(const Draw& a, const Draw& b) {
    return std::memcmp(&a, &b, sizeof(Draw)) == 0;
}

static std::vector<Axis> axes() {
    Axis r{ "rhythm",
        [](PE& p,int d){ p.advanceRhythmDraw(d); },
        [](PE& p){ p.zeroRhythmIndex(); },
        [](PE& p){ p.beginRhythmDraw(); },
        [](PE& p,float s){ p.seedRhythmPhilox(s); },
        [](PE& p){ p.seedRhythmPhiloxFull(); },
        [](PE& p){ return p.philoxRhythm(); },
        [](const PE& p){ return p.rhythmDrawCtr; },
        [](const PE& p){ return p.rhythmCursor; },
        [](const PE& p){ return p.rhythmDrawCache_.size(); },
        [](PE& p,int64_t pos){ (void)p.cachedRhythmDraw(pos); },
        [](PE& p,int64_t pos){ PE::RhythmDraw o; p.patternRhythmAt(pos,0.f,o); },
        [](PE& p,int64_t pos){ PE::RhythmDraw o; p.patternRhythmAt(pos,0.f,o);
                               return sameBits(o, p.cachedRhythmDraw(pos).draw); } };
    Axis m{ "melody",
        [](PE& p,int d){ p.advanceMelodyDraw(d); },
        [](PE& p){ p.zeroMelodyIndex(); },
        [](PE& p){ p.beginMelodyDraw(); },
        [](PE& p,float s){ p.seedMelodyPhilox(s); },
        [](PE& p){ p.seedMelodyPhiloxFull(); },
        [](PE& p){ return p.philoxMelody(); },
        [](const PE& p){ return p.melodyDrawCtr; },
        [](const PE& p){ return p.melodyCursor; },
        [](const PE& p){ return p.melodyDrawCache_.size(); },
        [](PE& p,int64_t pos){ (void)p.cachedMelodyDraw(pos); },
        [](PE& p,int64_t pos){ PE::MelodyDraw o; p.patternMelodyAt(pos,0.f,o); },
        [](PE& p,int64_t pos){ PE::MelodyDraw o; p.patternMelodyAt(pos,0.f,o);
                               return sameBits(o, p.cachedMelodyDraw(pos).draw); } };
    Axis q{ "qmix",
        [](PE& p,int d){ p.advanceQmixDraw(d); },
        [](PE& p){ p.zeroQmixIndex(); },
        [](PE& p){ p.beginQmixDraw(); },
        [](PE& p,float s){ p.seedQmixPhilox(s); },
        [](PE& p){ p.seedQmixPhiloxFull(); },
        [](PE& p){ return p.philoxQmix(); },
        [](const PE& p){ return p.qmixDrawCtr; },
        [](const PE& p){ return p.qmixCursor; },
        [](const PE& p){ return p.qmixDrawCache_.size(); },
        [](PE& p,int64_t pos){ (void)p.cachedQmixDraw(pos); },
        [](PE& p,int64_t pos){ PE::QmixDraw o; p.patternQmixAt(pos,0.f,o); },
        [](PE& p,int64_t pos){ PE::QmixDraw o; p.patternQmixAt(pos,0.f,o);
                               return sameBits(o, p.cachedQmixDraw(pos).draw); } };
    return { r, m, q };
}

// Seed all three with the SAME float (so only the stream-key separation can make
// them differ) and reset counter+cursor to a known baseline.
static PE freshAll(float seed = 0.f) {
    PE p;
    for (auto& a : axes()) { a.seedKeyed(p, seed); a.begin(p); }
    return p;
}

// Assert a quantity read from each axis is identical across all three.
template<class F>
static void lockstep(PE& p, F read, const char* what) {
    auto A = axes();
    auto v0 = read(A[0], p);
    for (size_t i = 1; i < A.size(); ++i) {
        auto vi = read(A[i], p);
        if (!(vi == v0)) {
            std::ostringstream s;
            s << what << " drifted: " << A[0].name << "=" << v0
              << " vs " << A[i].name << "=" << vi;
            throw std::runtime_error(s.str());
        }
    }
}

int main(){
    std::cout<<"\033[1mCA2 — per-axis lifecycle equivalence\033[0m\n";
    std::cout<<std::string(60,'=')<<"\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G1 — counter cadence is identical across the three axes");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G1a advance(dir) steps every axis's counter identically", {
        PE p = freshAll();
        // a mixed forward/reverse script — the signed counter must track the same
        const int dirs[] = {+1,+1,+1,-1,+1,-1,-1,-1,-1,+1,+1,+1,+1,-1};
        for (int d : dirs) {
            for (auto& a : axes()) a.advance(p, d);
            lockstep(p, [](Axis& a, PE& pe){ return a.ctr(pe); }, "drawCtr");
        }
        // and the value is the plain sum of sign(dir) — the shared cadence
        int64_t want = 0; for (int d : dirs) want += (d < 0 ? -1 : +1);
        for (auto& a : axes()) EXPECT_EQ(a.ctr(p), want);
    });
    TEST("G1b advance treats any dir>=0 as +1 and dir<0 as -1, uniformly", {
        PE p = freshAll();
        for (auto& a : axes()) { a.advance(p, 7); a.advance(p, 0); a.advance(p, -9); }
        for (auto& a : axes()) EXPECT_EQ(a.ctr(p), (int64_t)1);   // +1 +1 -1
        lockstep(p, [](Axis& a, PE& pe){ return a.ctr(pe); }, "drawCtr(magnitude-blind)");
    });
    TEST("G1c zero resets every axis's counter identically", {
        PE p = freshAll();
        for (auto& a : axes()) { a.advance(p, +1); a.advance(p, +1); a.advance(p, +1); }
        for (auto& a : axes()) a.zero(p);
        lockstep(p, [](Axis& a, PE& pe){ return a.ctr(pe); }, "drawCtr after zero");
        for (auto& a : axes()) EXPECT_EQ(a.ctr(p), (int64_t)0);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G2 — the intra-draw cursor behaves identically");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G2a begin() zeroes the cursor on every axis", {
        PE p = freshAll();
        for (auto& a : axes()) { a.philox(p); a.philox(p); a.philox(p); }   // bump cursors
        for (auto& a : axes()) a.begin(p);
        lockstep(p, [](Axis& a, PE& pe){ return a.cursor(pe); }, "cursor after begin");
        for (auto& a : axes()) EXPECT_EQ(a.cursor(p), (uint64_t)0);
    });
    TEST("G2b each philox draw bumps its cursor by exactly 1 (shared stride)", {
        PE p = freshAll();
        for (auto& a : axes()) a.begin(p);
        for (int n = 1; n <= 20; ++n) {
            for (auto& a : axes()) a.philox(p);
            lockstep(p, [](Axis& a, PE& pe){ return a.cursor(pe); }, "cursor stride");
            for (auto& a : axes()) EXPECT_EQ(a.cursor(p), (uint64_t)n);
        }
    });
    TEST("G2c begin() leaves the counter alone (cursor and counter are independent)", {
        PE p = freshAll();
        for (auto& a : axes()) { a.advance(p, +1); a.advance(p, +1); }   // ctr = 2
        for (auto& a : axes()) a.begin(p);
        for (auto& a : axes()) EXPECT_EQ(a.ctr(p), (int64_t)2);
        lockstep(p, [](Axis& a, PE& pe){ return a.ctr(pe); }, "drawCtr unaffected by begin");
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G3 — the draw cache fills and prunes identically");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G3a touching the same key set grows every cache to the same size", {
        PE p = freshAll();
        for (int64_t pos = 0; pos < 10; ++pos)
            for (auto& a : axes()) a.touchCache(p, pos);
        lockstep(p, [](Axis& a, PE& pe){ return a.cacheSize(pe); }, "cacheSize after fill");
        for (auto& a : axes()) EXPECT_EQ(a.cacheSize(p), (size_t)10);
    });
    TEST("G3b patternXAt prunes every cache to the same window around the same center", {
        // Load keys FAR outside the live window, then patternXAt(center) must prune
        // them away on every axis by the one shared prune (same center semantics).
        PE p = freshAll();
        const int64_t center = 1000;
        for (int64_t pos = 0; pos < 3000; pos += 250)   // spread well past the span, both sides
            for (auto& a : axes()) a.touchCache(p, pos);
        for (auto& a : axes()) a.patternAt0(p, center);
        // every axis now holds exactly the keys within [center-SPAN, center+SPAN]
        const int64_t span = PE::DRAW_CACHE_HALF_SPAN;
        size_t want = 0;
        for (int64_t pos = 0; pos < 3000; pos += 250)
            if (pos >= center - span && pos <= center + span) ++want;
        // patternAt0(center) inserts the center key; its K-window is NOT taken at
        // r==0 (slew=0 -> r=0 -> raw shortcut), so only `center` is added in-window.
        bool centerWasLoaded = (center % 250 == 0);
        size_t expect = want + (centerWasLoaded ? 0 : 1);
        lockstep(p, [](Axis& a, PE& pe){ return a.cacheSize(pe); }, "cacheSize after prune");
        for (auto& a : axes()) EXPECT_EQ(a.cacheSize(p), expect);
    });
    TEST("G3c the prune center tracks the argument identically (two centers, two axes' worth apart)", {
        PE p = freshAll();
        for (int64_t pos = -500; pos <= 500; pos += 50)
            for (auto& a : axes()) a.touchCache(p, pos);
        for (auto& a : axes()) a.patternAt0(p, 0);          // prune around 0
        lockstep(p, [](Axis& a, PE& pe){ return a.cacheSize(pe); }, "cacheSize @center 0");
        for (auto& a : axes()) a.patternAt0(p, 400);        // now around 400
        lockstep(p, [](Axis& a, PE& pe){ return a.cacheSize(pe); }, "cacheSize @center 400");
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G4 — the r==0 (slew=0) bit-identity shortcut is the shared contract");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G4a patternXAt(pos, slew=0) == cachedXDraw(pos).draw bitwise, every axis", {
        PE p = freshAll(3.5f);
        for (int64_t pos : {0, 1, 7, 42, 100})
            for (auto& a : axes())
                EXPECT(a.shortcutIsRaw(p, pos));
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G5 — reseed resets counter AND cache identically");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G5a keyed reseed zeroes the counter and clears the cache on every axis", {
        PE p = freshAll();
        for (auto& a : axes()) { a.advance(p,+1); a.advance(p,+1); a.touchCache(p, 0); a.touchCache(p, 1); }
        for (auto& a : axes()) a.seedKeyed(p, 9.f);
        lockstep(p, [](Axis& a, PE& pe){ return a.ctr(pe); }, "ctr after keyed reseed");
        lockstep(p, [](Axis& a, PE& pe){ return a.cacheSize(pe); }, "cache after keyed reseed");
        for (auto& a : axes()) { EXPECT_EQ(a.ctr(p), (int64_t)0); EXPECT_EQ(a.cacheSize(p), (size_t)0); }
    });
    TEST("G5b full reseed zeroes the counter and clears the cache on every axis", {
        PE p = freshAll();
        for (auto& a : axes()) { a.advance(p,+1); a.touchCache(p, 0); }
        for (auto& a : axes()) a.seedFull(p);
        lockstep(p, [](Axis& a, PE& pe){ return a.ctr(pe); }, "ctr after full reseed");
        lockstep(p, [](Axis& a, PE& pe){ return a.cacheSize(pe); }, "cache after full reseed");
        for (auto& a : axes()) { EXPECT_EQ(a.ctr(p), (int64_t)0); EXPECT_EQ(a.cacheSize(p), (size_t)0); }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G6 — the three streams are DISTINCT (the lifecycle is shared, NOT the RNG)");
    // ─────────────────────────────────────────────────────────────────────────
    // The content-difference must be REAL. The keyed seed site documents that the
    // three streams were ONCE collapsed onto one derivation ("patching the single
    // SEED input collapsed the two streams into one") — the deriveKey STREAM_* fix
    // separated them. If a future "simplification" re-collapses them, the lifecycle
    // pins above would ALL still pass (bookkeeping is identical) while the draws
    // silently became identical. This is the guard for exactly that.
    TEST("G6a same seed float, same (ctr,cursor): the three draws are pairwise distinct", {
        PE p = freshAll(2.0f);   // one seed float into all three keyed seeds
        float u[3];
        auto A = axes();
        for (size_t i = 0; i < A.size(); ++i) { A[i].begin(p); u[i] = A[i].philox(p); }
        EXPECT(u[0] != u[1]);   // rhythm vs melody
        EXPECT(u[1] != u[2]);   // melody vs qmix
        EXPECT(u[0] != u[2]);   // rhythm vs qmix
    });
    TEST("G6b the distinctness holds across several draws (not a one-index fluke)", {
        PE p = freshAll(2.0f);
        auto A = axes();
        for (auto& a : A) a.begin(p);
        int collisions01 = 0, collisions12 = 0, collisions02 = 0;
        for (int n = 0; n < 32; ++n) {
            float a0 = A[0].philox(p), a1 = A[1].philox(p), a2 = A[2].philox(p);
            if (a0 == a1) ++collisions01;
            if (a1 == a2) ++collisions12;
            if (a0 == a2) ++collisions02;
        }
        // independent uniforms: exact-equal collisions are astronomically unlikely
        EXPECT_EQ(collisions01, 0);
        EXPECT_EQ(collisions12, 0);
        EXPECT_EQ(collisions02, 0);
    });

    std::cout<<"\n"<<std::string(60,'=')<<"\n";
    std::cout<<(s_fail==0?"\033[32m":"\033[31m")<<s_pass<<" passed, "<<s_fail
             <<" failed\033[0m\n";
    return s_fail==0 ? 0 : 1;
}

/* ── MUTATION LOG (to run once the suite is green) ────────────────────────────
 * Each mutant built to its own binary, the build verified before the run.
 *   M1  advanceMelodyDraw steps by +2        → G1a FAILS (melody ctr drifts).
 *   M2  beginQmixDraw does not zero cursor    → G2a FAILS.
 *   M3  patternMelodyAt skips pruneDrawCache_ → G3b FAILS (melody cache unpruned).
 *   M4  seedQmixPhilox forgets ctr=0          → G5a FAILS.
 *   M5  seedMelodyPhilox uses STREAM_RHYTHM   → G6a FAILS (melody==rhythm) while
 *       EVERY lifecycle pin still passes — the exact re-collapse the site warns of.
 * ─────────────────────────────────────────────────────────────────────────── */
