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

    // ── R-4: tuple assembly vs the predicates' direct composition ───────────────────
    // The plan §2.5 row: RhythmDecisionCore::decide() must equal the predicates' direct
    // composition over EVERY MonoDecision x own-state x nv combination. On the star the
    // ceiling field is the container's own nv (the voice applies its draw under it at
    // nvIdxForVoice); gateType is the container's decision (D4's single-container case).
    SUITE("R-4 tuple — decide() == direct predicate composition (exhaustive 6x2x2 grid)");
    for (int i = 0; i < 6; ++i)
        for (int w = 0; w < 2; ++w)
            for (int t = 0; t < 2; ++t)
                for (int nv = 0; nv <= 6; nv += 3)
                    for (int h = 0; h < 2; ++h) {
                        const bool wasHeld = (w != 0), hadTail = (t != 0), held = (h != 0);
                        const ParentView p = pv(all[i], nv, held, wasHeld, hadTail);
                        std::ostringstream desc;
                        desc << decName(all[i]) << " nv=" << nv << " held=" << h
                             << " wasHeld=" << w << " hadTail=" << t;
                        TEST(desc.str(), {
                            const RhythmDecision rd = RhythmDecisionCore::decide(p);
                            EXPECT(rd.mayOnset      == onsetPermission(p));   // C1
                            EXPECT(rd.envelopeOpen  == envelopeAnd(p));       // C5
                            EXPECT(rd.forcedRest    == restClose(p));         // C2
                            EXPECT(rd.lengthCeiling == nv);                   // C3 ceiling = container nv
                            EXPECT(rd.holdFollow    == holdFollow(p));        // C4
                            EXPECT(rd.gateType      == all[i]);               // D4
                            // The branch-consumption contract the cascade relies on:
                            // forcedRest is exclusive with the landing/articulation reads —
                            // a rest tuple never carries a mid-gate follow or an onset.
                            if (rd.forcedRest) { EXPECT(!rd.mayOnset); }
                        });
                    }

    // ── R-5: the star as DATA — LEGACY_STAR_PARENTS ────────────────────────────────
    // The D2 legacy graph, declared as a constexpr word per voice: V1 (slot 0) is the root;
    // every other voice's sole container is V1 (exactly bit 0 set, nothing else).
    SUITE("R-5 star data — LEGACY_STAR_PARENTS is exactly the all-follow-V1 graph");
    TEST("root has no container", {
        EXPECT_EQ((int)RhythmDecisionCore::LEGACY_STAR_PARENTS[0], 0);
    });
    for (int v = 1; v < 16; ++v) {
        std::ostringstream desc;
        desc << "v" << (v + 1) << " -> {V1} only";
        TEST(desc.str(), {
            const uint16_t w = RhythmDecisionCore::LEGACY_STAR_PARENTS[v];
            EXPECT_EQ((int)w, 1);                       // bit 0 set, nothing else
        });
    }

    // ── R-5: parentsOf — the star source and the (inert-until-F) graph source ──────
    SUITE("R-5 parentsOf — star reads the data; GRAPH reads neutral until Stage F-1");
    {
        RhythmDecisionCore core;
        TEST("star: v0 -> 0, v1..v15 -> bit0", {
            EXPECT_EQ((int)core.parentsOf(0), 0);
            for (int v = 1; v < 16; ++v) EXPECT_EQ((int)core.parentsOf(v), 1);
        });
        TEST("star: out-of-range v is defensive-zero", {
            EXPECT_EQ((int)core.parentsOf(-1), 0);
            EXPECT_EQ((int)core.parentsOf(16), 0);
        });
        TEST("toggle: GRAPH -> all zero (D2: absent graph = free voices, INTENDED)", {
            core.setParentSource(RhythmDecisionCore::ParentSource::GRAPH);
            for (int v = 0; v < 16; ++v) EXPECT_EQ((int)core.parentsOf(v), 0);
            core.setParentSource(RhythmDecisionCore::ParentSource::LEGACY_STAR);
            EXPECT_EQ((int)core.parentsOf(3), 1);       // the toggle goes both ways
        });
    }

    // ── R-5: the forest fold == the single-parent composition on the star ──────────
    // The engine's actual call shape: rdm.decide(v, projection) must equal the per-container
    // static decide(p) for EVERY voice and EVERY ParentView, because the star's fold body
    // runs exactly once.
    SUITE("R-5 fold — rdm.decide(v, proj) == static decide(p) over the star (6x2x2x2 grid x 15 voices)");
    {
        RhythmDecisionCore core;
        for (int v = 1; v < 16; ++v)
            for (int i = 0; i < 6; ++i)
                for (int w = 0; w < 2; ++w)
                    for (int t = 0; t < 2; ++t)
                        for (int h = 0; h < 2; ++h) {
                            const ParentView p = pv(all[i], 3, h != 0, w != 0, t != 0);
                            const RhythmDecision folded =
                                core.decide(v, [&](int) { return p; });
                            const RhythmDecision direct = RhythmDecisionCore::decide(p);
                            TEST("fold == direct", {
                                EXPECT(folded.mayOnset      == direct.mayOnset);
                                EXPECT(folded.envelopeOpen  == direct.envelopeOpen);
                                EXPECT(folded.forcedRest    == direct.forcedRest);
                                EXPECT(folded.lengthCeiling == direct.lengthCeiling);
                                EXPECT(folded.holdFollow    == direct.holdFollow);
                                EXPECT(folded.gateType      == direct.gateType);
                            });
                        }
    }

    // ── R-5: the fold's multi-parent semantics, spec'd directly (the Stage-F preview) ──
    // Two synthetic containers through decideMask — the same fold core the engine runs:
    // D3 union for the onset, D1 intersection for the state terms, shortest ceiling,
    // D4 first-onsetting container names the gate type.
    SUITE("R-5 fold semantics — two parents through the bitmask (union/intersection/D4)");
    {
        const ParentView pa = pv(MonoDecision::NewNote, 2, true,  true,  false); // onsets, short window, holds
        const ParentView pb = pv(MonoDecision::Rest,    5, false, true,  true);  // rested, long window
        // parents {0,1}: bit0 = pa, bit1 = pb (projection dispatches by container index)
        auto projAB = [&](int u) { return u == 0 ? pa : pb; };
        auto projBA = [&](int u) { return u == 0 ? pb : pa; };
        TEST("D3 union: the NewNote container offers the onset despite the rested one", {
            const RhythmDecision d = RhythmDecisionCore::decideMask(0x0003, projAB);
            EXPECT(d.mayOnset);                      // events union (D3) — pa's attack counts
        });
        TEST("D1 intersection: one rested container closes the envelope", {
            const RhythmDecision d = RhythmDecisionCore::decideMask(0x0003, projAB);
            EXPECT(d.forcedRest);                    // any resting container closes (D1)
            EXPECT(!d.holdFollow && !d.envelopeOpen); // pb holds nothing -> the ANDs collapse
        });
        TEST("D1: the shortest container window wins the ceiling (max index)", {
            const RhythmDecision d = RhythmDecisionCore::decideMask(0x0003, projAB);
            EXPECT_EQ(d.lengthCeiling, 5);           // max(2, 5): the shorter window
        });
        TEST("D4: the FIRST ONSETTING container names the gate type (order-independent)", {
            // pa (NewNote) at bit 0 -> it is both first and first-onsetting:
            EXPECT(RhythmDecisionCore::decideMask(0x0003, projAB).gateType == MonoDecision::NewNote);
            // pa moved to bit 1 (pb the rested one is first): the first parent does NOT
            // onset, so the first ONSETTING container (pa) still names the verdict:
            EXPECT(RhythmDecisionCore::decideMask(0x0003, projBA).gateType == MonoDecision::NewNote);
        });
        TEST("empty mask -> the uncontained tuple (all neutral)", {
            const RhythmDecision d = RhythmDecisionCore::decideMask(0x0000, projAB);
            EXPECT(!d.mayOnset && !d.forcedRest && !d.holdFollow && !d.envelopeOpen);
            EXPECT_EQ(d.lengthCeiling, 0);
        });
    }

    std::cout << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail ? 1 : 0;
}
