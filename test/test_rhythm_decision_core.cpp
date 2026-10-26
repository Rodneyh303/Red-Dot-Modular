/**
 * test_rhythm_decision_core.cpp — the RDM predicate layer, table-driven (STAGE R R-3).
 *
 * Specs the five named predicates of RHYTHM_DECISION_MANAGER_PLAN §2.2/§2.3 (the C1–C5
 * mechanisms of VOICE1_CONTAINMENT_ALGORITHM.md's mechanism table) over every
 * MonoDecision × own-state combination:
 *   C1 onsetPermission — EVENTS: a container OFFERS a trigger (NewNote unconditionally;
 *      Legato/LegatoMax only from a dead state: !wasHeld && !hadTail — the isolated-teal
 *      guard). Stage F folds with UNION over containers (D3).
 *   C2 restClose       — STATES: the container rested → contained voices force-close and
 *      consult no draws. Stage F folds with INTERSECTION (any resting container closes) (D1).
 *   C3 lengthCeiling   — the voice never holds past the container's next event; NOTE_VALUES
 *      is ordered slowest→fastest, so the LARGER index (the shorter window) wins.
 *   C4 holdFollow      — mid-hold follow rides the container's gate; the voice's own armed
 *      window (gatePulseRemain/holdRemain) is the call site's, not the predicate's.
 *   C5 envelopeAnd     — the container's gate contributes one open cell to the output
 *      envelope; the voice's own latched candidacy (polyVoiceActive) is the call site's.
 *
 * The file grows with the ladder: R-4 adds tuple-assembly-vs-direct-composition rows and
 * the star-graph data; R-5 pins LEGACY_STAR as data.
 *
 * Rack-free: includes ONLY RhythmDecisionCore.hpp (the pure layer — no engine, no Rack).
 */
#include "RhythmDecisionCore.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>

static int s_pass = 0, s_fail = 0;
#define SUITE(n) do{std::cout<<"\n["<<(n)<<"]\n";}while(0)
#define TEST(desc,...) do{ bool _ok=true; std::string _m; \
    try{__VA_ARGS__;}catch(const std::exception&_e){_ok=false;_m=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  PASS "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  FAIL "<<(desc); if(!_m.empty())std::cout<<" — "<<_m; std::cout<<"\n";} }while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ")");}while(0)
#define EXPECT_EQ(a,b) do{if((a)!=(b)){std::ostringstream s;s<<#a<<"="<<(a)<<" != "<<(b);throw std::runtime_error(s.str());}}while(0)

static ParentView pv(MonoDecision d, int nv, bool held, bool wasHeld, bool tail) {
    return ParentView{ d, nv, held, wasHeld, tail };
}

static const char* decName(MonoDecision d) {
    switch (d) {
        case MonoDecision::MidNote:   return "MidNote";
        case MonoDecision::Rest:      return "Rest";
        case MonoDecision::Tie:       return "Tie";
        case MonoDecision::Legato:    return "Legato";
        case MonoDecision::LegatoMax: return "LegatoMax";
        case MonoDecision::NewNote:   return "NewNote";
    }
    return "?";
}

int main() {
    const MonoDecision all[6] = { MonoDecision::MidNote, MonoDecision::Rest, MonoDecision::Tie,
                                  MonoDecision::Legato, MonoDecision::LegatoMax, MonoDecision::NewNote };

    // ── C1 onsetPermission: the full 6 × 4 truth table ──────────────────────────────
    SUITE("C1 onsetPermission — every MonoDecision x dead-state combination");
    for (int i = 0; i < 6; ++i)
        for (int w = 0; w < 2; ++w)
            for (int t = 0; t < 2; ++t) {
                const bool wasHeld = (w != 0), hadTail = (t != 0);
                bool expected;
                switch (all[i]) {
                    case MonoDecision::NewNote:   expected = true; break;
                    case MonoDecision::Legato:
                    case MonoDecision::LegatoMax: expected = !wasHeld && !hadTail; break;
                    default:                      expected = false; break;
                }
                std::ostringstream desc;
                desc << decName(all[i]) << " wasHeld=" << w << " hadTail=" << t
                     << " -> " << (expected ? "onset" : "no onset");
                TEST(desc.str(), {
                    EXPECT_EQ((int)onsetPermission(pv(all[i], 3, true, wasHeld, hadTail)), (int)expected);
                });
            }

    // ── C2 restClose: exactly one decision closes ───────────────────────────────────
    SUITE("C2 restClose — Rest closes; every other decision does not");
    for (int i = 0; i < 6; ++i) {
        std::ostringstream desc;
        desc << decName(all[i]) << " -> " << (all[i] == MonoDecision::Rest ? "close" : "open");
        TEST(desc.str(), {
            EXPECT_EQ((int)restClose(pv(all[i], 3, true, true, true)),
                      (int)(all[i] == MonoDecision::Rest));
        });
    }

    // ── C3 lengthCeiling: the shorter window (larger NOTE_VALUES index) wins ────────
    SUITE("C3 lengthCeiling — max(own, parent) over the full 7x7 grid");
    for (int own = 0; own <= 6; ++own)
        for (int p = 0; p <= 6; ++p) {
            std::ostringstream desc;
            desc << "own=" << own << " parent=" << p << " -> ceiling=" << std::max(own, p);
            TEST(desc.str(), {
                EXPECT_EQ(lengthCeiling(own, pv(MonoDecision::MidNote, p, true, true, true)),
                          std::max(own, p));
                // Monotone containment: the ceiling never lengthens the voice past either side.
                EXPECT(lengthCeiling(own, pv(MonoDecision::MidNote, p, true, true, true)) >= own);
                EXPECT(lengthCeiling(own, pv(MonoDecision::MidNote, p, true, true, true)) >= p);
            });
        }

    // ── C4 holdFollow + C5 envelopeAnd: echo the container's gate ───────────────────
    SUITE("C4/C5 — both ride the container's gate; they agree on the single-parent star");
    for (int h = 0; h < 2; ++h) {
        const bool held = (h != 0);
        std::ostringstream desc;
        desc << "gateHeld=" << h << " -> holdFollow == envelopeAnd == " << h;
        TEST(desc.str(), {
            const ParentView p = pv(MonoDecision::MidNote, 3, held, false, false);
            EXPECT_EQ((int)holdFollow(p), h);
            EXPECT_EQ((int)envelopeAnd(p), h);
            // The degenerate-forest agreement (VOICE1_CONTAINMENT_ALGORITHM C5 row: in clock
            // mode C5 is IMPLICIT in C1+C2+C4 — one parent makes the two predicates the same).
            EXPECT_EQ((int)holdFollow(p), (int)envelopeAnd(p));
        });
    }

    // ── Own-state neutrality: predicates read ONLY the ParentView ──────────────────
    // (The voice's own state — wasHeldPoly/hadPolyTail/polyVoiceActive/draws — stays at the
    // call site; the predicates must not even LOOK at it, which a pure signature guarantees.)
    SUITE("signature purity — same ParentView, same verdict, always");
    TEST("C1/C2/C3/C4/C5 deterministic across repeated calls", {
        const ParentView p = pv(MonoDecision::Legato, 4, true, false, false);
        for (int r = 0; r < 8; ++r) {
            EXPECT(onsetPermission(p));
            EXPECT(!restClose(p));
            EXPECT_EQ(lengthCeiling(2, p), 4);
            EXPECT(holdFollow(p));
            EXPECT(envelopeAnd(p));
        }
    });

    std::cout << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail ? 1 : 0;
}
