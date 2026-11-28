#include "test_stubs.hpp"
/**
 * test_dice_undo.cpp  (G1 of the lock+undo sweep — PR #377 audit)
 * Unit tests for the dice-undo RESTORE contract — no Rack SDK needed.
 *
 * What this pins
 * --------------
 * A dice reroll snapshots (seedFloat, drawCtr) BEFORE and AFTER for each of the three
 * streams (rhythm / melody / q-mix). Undo replays "before"; redo replays "after". The
 * module's restore*Dice() helpers do exactly three lines per stream:
 *       pe.<x>SeedFloat = sf;
 *       pe.seed<X>Philox(sf);   // re-derives the Philox KEY from sf, and ZEROS drawCtr
 *       pe.<x>DrawCtr   = ctr;  // ORDER MATTERS: ctr is set AFTER, because seeding zeroed it
 * The pattern is a pure function of (Philox key, drawCtr), and the key is a pure function
 * of seedFloat via deriveKey(). So restoring (seedFloat, drawCtr) must reproduce the exact
 * pre-roll draw stream. These helpers live on Monsoon (Rack module); they are replicated
 * verbatim below so the contract is tested rack-free against a bare PatternEngine.
 *
 * NOT covered here: the lock-free SPSC diceUndoRing (audio->UI handoff) and the UI-thread
 * DiceUndoAction push — pure plumbing on the Rack module, left to feel-test.
 *
 * Compile: see test/run_all.sh  (group $PE)
 */
#include "PatternEngine.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <cmath>

// ── Test framework ────────────────────────────────────────────────────────────
static int s_pass=0, s_fail=0;
#define SUITE(n) do{std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n";}while(0)
#define TEST(desc,...) do{ \
    bool _ok=true; std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31m✗\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" — "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e)     do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_EQ(a__,b__) do{if((a__)!=(b__)){std::ostringstream _s; \
    _s<<#a__<<"="<<(a__)<<" != "<<#b__<<"="<<(b__);throw std::runtime_error(_s.str());}}while(0)

// ── restore helpers: byte-for-byte mirror of Monsoon::restore{Rhythm,Melody,Qmix}Dice ──
static void restoreRhythm(PatternEngine& pe, float sf, int64_t ctr){
    pe.rhythmSeedFloat = sf; pe.seedRhythmPhilox(sf); pe.rhythmDrawCtr = ctr;
}
static void restoreMelody(PatternEngine& pe, float sf, int64_t ctr){
    pe.melodySeedFloat = sf; pe.seedMelodyPhilox(sf); pe.melodyDrawCtr = ctr;
}
static void restoreQmix(PatternEngine& pe, float sf, int64_t ctr){
    pe.qmixSeedFloat = sf; pe.seedQmixPhilox(sf); pe.qmixDrawCtr = ctr;
}

// ── deterministic fingerprint of a stream at its CURRENT (key, drawCtr) ─────────
// philox*At is a pure const draw at (pos, cursor): folds in the key (seed) AND the
// current drawCtr, so an equal fingerprint proves an identical generated pattern.
static std::vector<float> fpRhythm(const PatternEngine& pe){
    std::vector<float> v; for(uint64_t k=0;k<8;++k) v.push_back(pe.philoxRhythmAt(pe.rhythmDrawCtr,k)); return v;
}
static std::vector<float> fpMelody(const PatternEngine& pe){
    std::vector<float> v; for(uint64_t k=0;k<8;++k) v.push_back(pe.philoxMelodyAt(pe.melodyDrawCtr,k)); return v;
}
static std::vector<float> fpQmix(const PatternEngine& pe){
    std::vector<float> v; for(uint64_t k=0;k<8;++k) v.push_back(pe.philoxQmixAt(pe.qmixDrawCtr,k)); return v;
}

int main(){
    SUITE("dice undo — rhythm round-trip");
    TEST("undo restores exact pre-roll (seed,ctr) and pattern", {
        PatternEngine pe;
        // pre-roll state
        restoreRhythm(pe, 0.321f, 5);
        const float   sBefore = pe.rhythmSeedFloat; const int64_t cBefore = pe.rhythmDrawCtr;
        const auto    fpBefore = fpRhythm(pe);
        // a reroll: new seed (slew morph) + new position
        restoreRhythm(pe, 0.777f, 9);
        const auto    fpAfter = fpRhythm(pe);
        EXPECT(fpBefore != fpAfter);                 // the roll actually changed the pattern
        // UNDO = replay "before"
        restoreRhythm(pe, sBefore, cBefore);
        EXPECT_EQ(pe.rhythmSeedFloat, 0.321f);
        EXPECT_EQ(pe.rhythmDrawCtr, (int64_t)5);
        EXPECT(fpRhythm(pe) == fpBefore);            // exact pattern restored
    });

    TEST("redo restores exact post-roll (seed,ctr) and pattern", {
        PatternEngine pe;
        restoreRhythm(pe, 0.321f, 5);
        restoreRhythm(pe, 0.777f, 9);
        const auto fpAfter = fpRhythm(pe);
        restoreRhythm(pe, 0.321f, 5);                // undo
        restoreRhythm(pe, 0.777f, 9);                // redo
        EXPECT_EQ(pe.rhythmDrawCtr, (int64_t)9);
        EXPECT(fpRhythm(pe) == fpAfter);
    });

    SUITE("dice undo — ORDER MATTERS (seed zeros the counter)");
    TEST("restored counter survives the seed call (not clobbered to 0)", {
        PatternEngine pe;
        restoreRhythm(pe, 0.42f, 7);
        EXPECT_EQ(pe.rhythmDrawCtr, (int64_t)7);     // would be 0 if ctr were set before seeding
        // prove the counter placement is observable in the pattern: ctr 7 != ctr 0
        const auto fpAt7 = fpRhythm(pe);
        pe.rhythmDrawCtr = 0;
        EXPECT(fpRhythm(pe) != fpAt7);
    });

    SUITE("dice undo — stream independence");
    TEST("restoring rhythm leaves melody and q-mix untouched", {
        PatternEngine pe;
        restoreRhythm(pe, 0.11f, 3);
        restoreMelody(pe, 0.22f, 4);
        restoreQmix  (pe, 0.33f, 6);
        const auto mBefore = fpMelody(pe);
        const auto qBefore = fpQmix(pe);
        // undo a rhythm roll only
        restoreRhythm(pe, 0.99f, 1);
        restoreRhythm(pe, 0.11f, 3);
        EXPECT(fpMelody(pe) == mBefore);
        EXPECT(fpQmix(pe)   == qBefore);
        EXPECT_EQ(pe.melodyDrawCtr, (int64_t)4);
        EXPECT_EQ(pe.qmixDrawCtr,   (int64_t)6);
    });

    TEST("melody and q-mix restore round-trip independently", {
        PatternEngine pe;
        restoreMelody(pe, 0.15f, 2); const auto m0 = fpMelody(pe);
        restoreQmix  (pe, 0.85f, 8); const auto q0 = fpQmix(pe);
        restoreMelody(pe, 0.55f, 5); EXPECT(fpMelody(pe) != m0);
        restoreQmix  (pe, 0.05f, 1); EXPECT(fpQmix(pe)   != q0);
        restoreMelody(pe, 0.15f, 2); restoreQmix(pe, 0.85f, 8);
        EXPECT(fpMelody(pe) == m0);
        EXPECT(fpQmix(pe)   == q0);
    });

    std::cout<<"\n"<<(s_fail? "\033[31m":"\033[32m")<<s_pass<<" passed, "<<s_fail<<" failed\033[0m\n";
    return s_fail? 1:0;
}
