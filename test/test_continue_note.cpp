#include "test_stubs.hpp"
/**
 * test_continue_note.cpp
 * THE ONE JOIN — GateState::continueNote (the tie/legato collapse).
 *
 * WHAT THIS PINS. Before the collapse the join was two functions chosen by the
 * CALLER: extendHold (tie) and slideNote (slide). extendHold never wrote
 * currentPitchV or lastSemitone, so a join classed Tie DISCARDED the pitch it had
 * just drawn. That was benign only because every caller happened to pair it with
 * the strict same-degree-AND-octave test — a coincidence of discipline, not a
 * property of the code, and it had already broken once (the degree-only compare
 * silently dropped the new octave; CC_PROMPT_POLY_TIE_EMERGENT).
 *
 * continueNote makes that class UNREPRESENTABLE: the drawn pitch and degree are
 * written on EVERY path, and the Tie-vs-Legato label is DERIVED from the same
 * predicate the action uses, then returned so no caller repeats the test. These
 * gates pin the invariant itself, not the old pair's incidental behaviour — see
 * the mutation log at the foot of this file for what each one catches.
 *
 * Compile: see test/run_all.sh (the registry is the source of truth).
 */
#include "GateState.hpp"
#include <iostream>
#include <sstream>
#include <cstring>

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

static GateState fresh() { GateState g; g.reset(); return g; }

using NT = GateState::NoteType;

// A cent in 1V/oct = 1/1200 V. The epsilon is deliberately BELOW this: it admits
// numerical dust and nothing a user can author.
static constexpr float kCentV = 1.f / 1200.f;           // 8.333e-4 V
static constexpr float kEps   = GateState::kPitchEps;   // 1e-4 V ~ 0.12 cents

// nvIdx 7 = 1/8 = 2 steps in this table; nvIdx 4 = 1/4 = 4 steps.
static constexpr int NV8 = 7, NV4 = 4;

int main(){
    std::cout<<"\033[1mcontinueNote — THE ONE JOIN\033[0m\n";
    std::cout<<std::string(60,'=')<<"\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G1 — ALWAYS WRITES the drawn pitch (the octave-discard class)");
    // ─────────────────────────────────────────────────────────────────────────
    // The old extendHold left currentPitchV at the HELD value. Within the epsilon
    // that was inaudible; the point is that it was structurally possible at all.
    // Here the join IS a tie (sub-epsilon delta) and the stored pitch must still be
    // the value drawn, BIT-EXACTLY — the invariant that makes a future loosening of
    // the predicate safe instead of a silent discard.
    TEST("G1a sub-epsilon tie: stores the DRAWN pitch, bit-exact", {
        auto g=fresh(); g.triggerNote(1.f, 5, NV8);
        const float drawn = 1.f + 0.5f*kEps;
        const NT lbl = g.continueNote(drawn, 5, NV8, /*wasHeld=*/true);
        EXPECT(lbl == NT::Tie);                 // still a tie: inside the epsilon
        EXPECT(g.currentPitchV == drawn);       // and the drawn value landed
    });
    TEST("G1b same degree, NEW OCTAVE: slides AND the octave lands", {
        // The original bug: degree-only compare classed this a Tie and the new
        // octave vanished. The drawn pitch must land and the label must be Legato.
        auto g=fresh(); g.triggerNote(1.f, 5, NV8);
        const NT lbl = g.continueNote(2.f, 5, NV8, /*wasHeld=*/true);
        EXPECT(lbl == NT::Legato);
        EXPECT(g.currentPitchV == 2.f);
    });
    TEST("G1c a RETUNED degree (one cent) slides to the new tuning", {
        // TIE_UNDER_MICROTONALITY: a cent is 8x the epsilon, so retuning a degree
        // is NOT a tie — the gate continues and the pitch follows the retuning.
        auto g=fresh(); g.triggerNote(1.f, 5, NV8);
        const float retuned = 1.f + kCentV;
        const NT lbl = g.continueNote(retuned, 5, NV8, /*wasHeld=*/true);
        EXPECT(lbl == NT::Legato);
        EXPECT(g.currentPitchV == retuned);
        EXPECT(g.gateHeld);                     // continues — it SLIDES, no re-strike
    });
    TEST("G1d lastSemitone always written: the NEXT compare uses the new degree", {
        // A chain property. If a join left lastSemitone stale, the following join
        // would compare against a degree that is no longer sounding.
        auto g=fresh(); g.triggerNote(1.f, 5, NV8);
        g.continueNote(2.f, 7, NV8, true);      // slide to degree 7
        EXPECT_EQ(g.lastSemitone, 7);
        EXPECT(g.continueNote(2.f, 7, NV8, true) == NT::Tie);   // now 7 ties
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G2 — the label EMERGES (predicate and action are one unit)");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G2a the returned label == lastNoteType (no second source of truth)", {
        auto g=fresh(); g.triggerNote(1.f, 5, NV8);
        EXPECT(g.continueNote(1.f, 5, NV8, true) == g.lastNoteType);
        EXPECT(g.continueNote(3.f, 9, NV8, true) == g.lastNoteType);
    });
    TEST("G2b the label == isSameAsHeld measured BEFORE the call", {
        struct Case { float held; int heldSem; float drawn; int sem; };
        const Case cs[] = {
            {1.f, 5, 1.f,            5},   // identical          → Tie
            {1.f, 5, 1.f+0.5f*kEps,  5},   // dust               → Tie
            {1.f, 5, 1.f+2.f*kEps,   5},   // past the epsilon   → Legato
            {1.f, 5, 1.f+kCentV,     5},   // one cent           → Legato
            {1.f, 5, 2.f,            5},   // octave             → Legato
            {1.f, 5, 1.f,            6},   // same V, new degree → Legato
            {0.f, 0, 0.f,            0},   // zero/zero          → Tie
        };
        for (const Case& c : cs) {
            auto g=fresh(); g.triggerNote(c.held, c.heldSem, NV8);
            const bool want = g.isSameAsHeld(c.drawn, c.sem);   // measured first
            const NT got = g.continueNote(c.drawn, c.sem, NV8, true);
            EXPECT((got == NT::Tie) == want);
        }
    });
    TEST("G2c DEGREE equality is required (a duplicate-pitch scale slides)", {
        // A user scale with two degrees at the same voltage, or a muted-degree
        // quantize fold: the voltage matches but the degree moved → a slide.
        auto g=fresh(); g.triggerNote(1.f, 5, NV8);
        EXPECT(g.continueNote(1.f, 6, NV8, true) == NT::Legato);
        EXPECT_EQ(g.lastSemitone, 6);
    });
    TEST("G2d the epsilon boundary, both sides", {
        {   auto g=fresh(); g.triggerNote(1.f, 5, NV8);
            EXPECT(g.continueNote(1.f + 0.99f*kEps, 5, NV8, true) == NT::Tie);   }
        {   auto g=fresh(); g.triggerNote(1.f, 5, NV8);
            EXPECT(g.continueNote(1.f + 1.01f*kEps, 5, NV8, true) == NT::Legato); }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G3 — a tie requires a HELD predecessor (wasHeld is not decorative)");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G3a cold open with a matching pitch is SINGLE, not Tie", {
        // Nothing is sounding, so there is nothing to tie TO, however well the
        // drawn pitch matches the stale stored value.
        auto g=fresh(); g.triggerNote(1.f, 5, NV8);
        g.gatePulse.reset();
        const NT lbl = g.continueNote(1.f, 5, NV8, /*wasHeld=*/false);
        EXPECT(lbl == NT::Single);
        EXPECT(g.gatePulse.isHigh());           // it re-strikes
    });
    TEST("G3b cold open SETS the hold (does not sum)", {
        auto g=fresh(); g.triggerNote(1.f, 5, NV4);   // 4 steps
        g.continueNote(1.f, 5, NV8, /*wasHeld=*/false);
        EXPECT_NEAR(g.holdRemain, 2.f, 1e-4f);        // SET to 2, not 4+2
    });
    TEST("G3c a held join SUMS the hold and does NOT re-strike", {
        auto g=fresh(); g.triggerNote(1.f, 5, NV4);   // 4 steps
        g.gatePulse.reset();
        g.continueNote(1.f, 5, NV8, /*wasHeld=*/true);
        EXPECT_NEAR(g.holdRemain, 6.f, 1e-4f);        // 4 + 2
        EXPECT(!g.gatePulse.isHigh());
        EXPECT(g.gateHeld);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G4 — the LED rule, derived from the SAME predicate");
    // ─────────────────────────────────────────────────────────────────────────
    // Visual only (semiPlayRemain drives the fader flash), but the two legs are
    // genuinely different and the old pair had them right: a TIE is ONE sustained
    // note, so its fader stays lit for the whole remaining hold; a SLIDE lights
    // the new degree for that note's own length. Feeding the summed hold to a
    // slide would smear every fader in a legato run.
    TEST("G4a tie: the fader is lit for the SUMMED hold", {
        auto g=fresh(); g.triggerNote(1.f, 5, NV4);   // dur 4 → semi[5] = 4
        g.tick();                                     // hold 4→3, semi[5] 4→3
        g.continueNote(1.f, 5, NV8, true);            // +2 → hold 5
        EXPECT_NEAR(g.holdRemain, 5.f, 1e-4f);
        EXPECT_NEAR(g.semiPlayRemain[5], 5.f, 1e-4f); // the whole remaining hold
    });
    TEST("G4b slide: the NEW fader is lit for THIS note's length only", {
        auto g=fresh(); g.triggerNote(1.f, 5, NV4);
        g.tick();
        g.continueNote(2.f, 7, NV8, true);            // slide to degree 7, dur 2
        EXPECT_NEAR(g.holdRemain, 5.f, 1e-4f);        // hold still sums
        EXPECT_NEAR(g.semiPlayRemain[7], 2.f, 1e-4f); // but the flash is dur, not 5
    });
    TEST("G4c isQuant routes the flash to the quant timer on BOTH legs", {
        {   auto g=fresh(); g.triggerNote(1.f, 5, NV8, /*isQuant=*/true);
            g.continueNote(1.f, 5, NV8, true, /*isQuant=*/true);     // tie
            EXPECT(g.semiQuantPlayRemain[5] > 0.f);
            EXPECT_NEAR(g.semiPlayRemain[5], 0.f, 1e-6f);   }
        {   auto g=fresh(); g.triggerNote(1.f, 5, NV8, /*isQuant=*/true);
            g.continueNote(2.f, 7, NV8, true, /*isQuant=*/true);     // slide
            EXPECT(g.semiQuantPlayRemain[7] > 0.f);
            EXPECT_NEAR(g.semiPlayRemain[7], 0.f, 1e-6f);   }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G5 — the epsilon's two bounds (why 1e-4 and not something else)");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G5a above float/table dust: 1e-7 V apart still ties", {
        auto g=fresh(); g.triggerNote(1.f, 5, NV8);
        EXPECT(g.continueNote(1.f + 1e-7f, 5, NV8, true) == NT::Tie);
    });
    TEST("G5b below anything a user can author: eps < one cent", {
        EXPECT(GateState::kPitchEps < kCentV);
        // and by a real margin — not a coincidence of rounding
        EXPECT(GateState::kPitchEps * 8.f < kCentV);
    });
    TEST("G5c isSamePitch is symmetric in its two pitches", {
        EXPECT(GateState::isSamePitch(1.f, 1.f + 0.5f*kEps, 5, 5)
            == GateState::isSamePitch(1.f + 0.5f*kEps, 1.f, 5, 5));
        EXPECT(GateState::isSamePitch(1.f, 1.f + 2.f*kEps, 5, 5)
            == GateState::isSamePitch(1.f + 2.f*kEps, 1.f, 5, 5));
    });

    std::cout<<"\n"<<std::string(60,'=')<<"\n";
    std::cout<<(s_fail==0?"\033[32m":"\033[31m")<<s_pass<<" passed, "<<s_fail
             <<" failed\033[0m\n";
    return s_fail==0 ? 0 : 1;
}

/* ── MUTATION LOG ─────────────────────────────────────────────────────────────
 * Each mutant built to its own binary and the build VERIFIED before running (the
 * stale-binary lesson). Recorded in the run_all.sh registry entry too.
 *
 *  M1  restore the omission: `if (!tie) currentPitchV = pitchV;`
 *        → G1a FAILS. This is the collapse's whole point: the old split's
 *          behaviour is now a detectable mutant rather than the shipped code.
 *  M2  drop `wasHeld &&` from the tie predicate
 *        → SURVIVES — STRUCTURAL, and verified by reading the code back rather
 *          than assumed: `tie` is read ONLY inside `if (wasHeld)`, so on a cold
 *          open it is computed and discarded; the Single label comes from the
 *          branch, not the predicate. No test can separate the two, which is what
 *          "structural" means here. The conjunct is kept anyway: it is the guard
 *          that would matter if continueNote ever returned the bool instead of
 *          lastNoteType, and G3a pins the behaviour it protects.
 *  M3  markSemi always `dur`  (the old slideNote leg applied to both)
 *        → G4a FAILS.
 *  M4  markSemi always `holdRemain`  (the old extendHold leg applied to both)
 *        → G4b FAILS.
 *  M5  `if (!tie) lastSemitone = semitone;`
 *        → SURVIVES, and that is correct: the tie predicate CONTAINS
 *          sem == lastSemitone, so writing it on a tie is provably a no-op.
 *          Kept unconditional because the no-op depends on the predicate, and
 *          the next person to widen the predicate should not have to notice.
 * ─────────────────────────────────────────────────────────────────────────── */
