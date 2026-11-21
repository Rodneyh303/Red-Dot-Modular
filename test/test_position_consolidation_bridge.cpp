// test_position_consolidation_bridge.cpp — POSITION_RESOLVER_CONSOLIDATION.md's
// step-1 discipline: a BYTE-IDENTICAL bridge test proving the extraction changes
// NOTHING vs what ships, before any behaviour change (how SandsModifierResolver
// landed spread/warp/LOR/dir, and how PositionResolver slice 1 was proven).
//
// Every value the consolidation introduced is a PURE RE-EXPRESSION of the shipped
// inline formulas; this test sweeps the full grid and pins bit-identity:
//   1. THE FOLD      — SequencerEngine::walkFold(d,t,len) ==
//                      laneTickFor(d,t,len) + anchorBias(d)          (the ~10-site fan-out's core)
//   2. THE POLICIES  — the four WalkPolicies' anchorBias() ==
//                      SequencerEngine::anchorBias(d), and the entry edges are the
//                      shipped wEdge law (LEFT 0 / RIGHT len-1, keyed Reverse vs else)
//   3. THE EDGE      — walkEdge(d,len) == (d==Reverse ? len-1 : 0)  (voiceFrameEdgeLive_'s wEdge)
//   4. THE SLOT      — PositionResolver::playheadSlotBar == the shipped CC_PROMPT_
//                      REVERSE_VOICE_ROTATION_EDGE expression (moved verbatim out of
//                      SandsRangeGestureCore), and the ENTRY-EDGE PROPERTY holds
//                      through the POLICY face for every len/rot/off: at the entry
//                      frame value (readCell = off + rot + bias) the slot bar is the
//                      direction-entry edge — LEFT forward / RIGHT reverse
//   5. THE COORDINATE — PositionResolver::signedPhraseCoord ==
//                      voiceFrameEdgeLive_'s shipped inline vCoord
// The AUDIO byte-identity is proven by the whole battery passing unchanged — the
// reads flow through the migrated sites, so any drift would fail the suites and the
// goldens (the established deletion-safety proof).
//
// THE CONCEPT BITE (the doc's payoff) is compile-time: static_assert that all four
// policies satisfy WalkPolicy — an incomplete policy ("reverse forgot its
// right-edge rule") is a LOCATED compile error at its declaration, not a
// feel-test discovery.
//
// Compile: see test/run_all.sh ("test_position_consolidation_bridge").
#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
using dotModular::VoiceResolver;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

// The shipped expressions, verbatim (the bridge's mirrors).
static int64_t shipWrapMod(int64_t x, int64_t m) { int64_t r = x % m; if (r < 0) r += m; return r; }
static int64_t shipFold(SequencerEngine::LaneDir d, int64_t t, int len) {
    return SequencerEngine::laneTickFor(d, t, len) + SequencerEngine::anchorBias(d);
}
static int64_t shipEdge(SequencerEngine::LaneDir d, int len) {
    return (d == SequencerEngine::LaneDir::Reverse) ? (int64_t)(len - 1) : (int64_t)0;
}
static int64_t shipSlot(int64_t readCell, int len, int64_t off, int64_t rot, int64_t bias, int ring) {
    // THE LAW'S SPEC: the in-window frame value recovered FIRST, then the unmixing.
    const int64_t l = (len < 1) ? 1 : len;
    const int64_t o = shipWrapMod(off, ring);
    const int64_t wnow = shipWrapMod(readCell - o, ring);   // in [0, len): the read cell is in-window
    return shipWrapMod(o + shipWrapMod(wnow - rot - bias, l), ring);
}
// The T1-SHIPPED expression (the pre-wrap form the consolidation moved) — the DELTA
// reference: it agrees with the law everywhere EXCEPT ring-wrapping windows whose
// len does not divide the ring (the ring wrap shifts the read cell by k·ring ≢ 0
// mod len and the box-offset lands off by the residue) — a latent T1 defect the
// bridge's off=31 / len=3 grid cells exposed, fixed in the moved law (pinned below).
static int64_t t1ShippedSlot(int64_t readCell, int len, int64_t off, int64_t rot, int64_t bias, int ring) {
    const int64_t l = (len < 1) ? 1 : len;
    const int64_t o = shipWrapMod(off, ring);
    return shipWrapMod(o + shipWrapMod(shipWrapMod(readCell, ring) - o - rot - bias, l), ring);
}
static int64_t shipCoord(int64_t phrasePos, int dirSign, int64_t advanceOffset, int64_t lenG) {
    return shipWrapMod(((dirSign > 0) ? phrasePos : -phrasePos) + advanceOffset, lenG);
}

int main() {
    std::printf("test_position_consolidation_bridge: the walk-law extraction is bit-identical to what ships (fold/edge/slot/coordinate, full grid) + the concept bites on all four policies\n");
    char msg[256];

    // THE CONCEPT BITE — compile-time completeness of all four policies.
    static_assert(WalkPolicy<SequencerEngine::WalkFwd>,  "WalkFwd incomplete");
    static_assert(WalkPolicy<SequencerEngine::WalkRev>,  "WalkRev incomplete");
    static_assert(WalkPolicy<SequencerEngine::WalkPend>, "WalkPend incomplete");
    static_assert(WalkPolicy<SequencerEngine::WalkPP>,   "WalkPP incomplete");

    const SequencerEngine::LaneDir dirs[4] = {
        SequencerEngine::LaneDir::Forward, SequencerEngine::LaneDir::Reverse,
        SequencerEngine::LaneDir::Pendulum, SequencerEngine::LaneDir::PingPong };
    const char* dirName[4] = { "Fwd", "Rev", "Pend", "PP" };
    const int lens[] = { 1, 3, 8, 16, 32 };

    // 1+2+3. THE FOLD / THE EDGE / THE POLICY CONSTANTS — full grid, negative-safe ticks.
    long foldChecks = 0;
    for (int di = 0; di < 4; ++di) {
        const auto d = dirs[di];
        for (int len : lens) {
            for (int64_t t = -17; t <= 40; ++t) {
                const int64_t got = SequencerEngine::walkFold(d, t, len);
                const int64_t want = shipFold(d, t, len);
                if (got != want) {
                    std::snprintf(msg, sizeof msg, "FOLD %s t=%lld len=%d: walkFold %lld != shipped %lld",
                                  dirName[di], (long long)t, len, (long long)got, (long long)want);
                    CHECK(false, msg);
                }
                ++foldChecks;
            }
            const int64_t e = SequencerEngine::walkEdge(d, len);
            const int64_t eWant = shipEdge(d, len);
            if (e != eWant) {
                std::snprintf(msg, sizeof msg, "EDGE %s len=%d: walkEdge %lld != shipped %lld",
                              dirName[di], len, (long long)e, (long long)eWant);
                CHECK(false, msg);
            }
        }
        // THE POLICIES' declared constants == the runtime anchorBias/edge law (the
        // pinned twins — the same constant the read law folds, declared once per
        // policy so an incomplete declaration cannot compile).
        const int64_t pb =
            (di == 0) ? SequencerEngine::WalkFwd::anchorBias() :
            (di == 1) ? SequencerEngine::WalkRev::anchorBias() :
            (di == 2) ? SequencerEngine::WalkPend::anchorBias() :
                        SequencerEngine::WalkPP::anchorBias();
        const int64_t pbWant = SequencerEngine::anchorBias(d);
        if (pb != pbWant) {
            std::snprintf(msg, sizeof msg, "POLICY %s: anchorBias %lld != the runtime constant %lld",
                          dirName[di], (long long)pb, (long long)pbWant);
            CHECK(false, msg);
        }
        const int pe =
            (di == 0) ? (int)SequencerEngine::WalkFwd::entryEdge(8) :
            (di == 1) ? (int)SequencerEngine::WalkRev::entryEdge(8) :
            (di == 2) ? (int)SequencerEngine::WalkPend::entryEdge(8) :
                        (int)SequencerEngine::WalkPP::entryEdge(8);
        const int peWant = (int)shipEdge(d, 8);
        if (pe != peWant) {
            std::snprintf(msg, sizeof msg, "POLICY %s: entryEdge %d != the shipped law %d",
                          dirName[di], pe, peWant);
            CHECK(false, msg);
        }
    }
    std::printf("  fold grid: %ld (dir x len x t) bit-identical + edges + policy constants\n", foldChecks);

    // 4. THE SLOT — the primitives face vs the shipped expression, swept.
    long slotChecks = 0;
    for (int len : {1, 3, 8, 16}) {
        for (int64_t off : {0, 7, 31}) {
            for (int64_t rot : {0, 2, 5}) {
                for (int64_t bias : {0, 1}) {
                    for (int64_t r = -8; r < 64; ++r) {
                        const int64_t got = PositionResolver::playheadSlotBar(r, len, off, rot, bias, dotModular::kMaskRing);
                        const int64_t want = shipSlot(r, len, off, rot, bias, dotModular::kMaskRing);
                        if (got != want) {
                            std::snprintf(msg, sizeof msg, "SLOT r=%lld len=%d off=%lld rot=%lld bias=%lld: %lld != shipped %lld",
                                          (long long)r, len, (long long)off, (long long)rot, (long long)bias,
                                          (long long)got, (long long)want);
                            CHECK(false, msg);
                        }
                        ++slotChecks;
                    }
                }
            }
        }
    }
    std::printf("  slot grid: %ld (len x off x rot x bias x readCell) bit-identical\n", slotChecks);

    // 4a-delta. THE T1-SHIPPED EXPRESSION vs the moved law — the pinned blast radius,
    // cell-exact. The law's DOMAIN is in-window read cells (frameReadCell guarantees
    // the read cell sits in [off, off+len) ring-wrapped); within that domain:
    //   divergence ⟺ (the cell CROSSES the ring origin: readCell < off)
    //                AND (ring % len != 0)
    // — the crossing cell's read-cell − off wraps by k·ring ≢ 0 (mod len), which the
    // T1 pre-wrap form leaves in the box-offset (the latent T1 defect the bridge's
    // off=31 grids exposed) and the recovery form corrects. Every crossing cell in a
    // non-dividing window MUST diverge; every other in-window cell MUST be
    // bit-identical (incl. the whole non-wrapping domain — the T1 test's off {0,2} x
    // len 8 sweep — and every len-dividing window).
    long agree = 0, diverge = 0;
    for (int len : {1, 3, 8, 16, 32}) {
        for (int64_t off : {0, 7, 31}) {
            const int64_t o = shipWrapMod(off, dotModular::kMaskRing);
            for (int i = 0; i < len; ++i) {
                const int64_t r = (o + i) % dotModular::kMaskRing;   // the in-window cell
                for (int64_t rot : {0, 2, 5}) {
                    for (int64_t bias : {0, 1}) {
                        const int64_t got = PositionResolver::playheadSlotBar(r, len, off, rot, bias, dotModular::kMaskRing);
                        const int64_t t1 = t1ShippedSlot(r, len, off, rot, bias, dotModular::kMaskRing);
                        const bool wantDiverge = (r < o) && (dotModular::kMaskRing % len != 0);
                        if (wantDiverge) {
                            ++diverge;
                            if (got == t1) {
                                std::snprintf(msg, sizeof msg, "DELTA: crossing cell r=%lld len=%d off=%lld rot=%lld bias=%lld AGREES with the T1 form — the pinned defect did not bite",
                                              (long long)r, len, (long long)off, (long long)rot, (long long)bias);
                                CHECK(false, msg);
                            }
                        } else {
                            ++agree;
                            if (got != t1) {
                                std::snprintf(msg, sizeof msg, "DELTA outside the pinned class: r=%lld len=%d off=%lld rot=%lld bias=%lld (law %lld, T1 %lld)",
                                              (long long)r, len, (long long)off, (long long)rot, (long long)bias,
                                              (long long)got, (long long)t1);
                                CHECK(false, msg);
                            }
                        }
                    }
                }
            }
        }
    }
    std::printf("  T1 delta: %ld in-window cells agree, %ld diverge — divergence ⟺ (cell crosses the ring origin) && (ring %% len != 0)\n", agree, diverge);
    CHECK(diverge > 0, "the delta class is non-empty (the pinned defect is real)");

    // 4b. THE ENTRY-EDGE PROPERTY through the POLICY face — the T1 law's soul, now
    // per-policy: at the entry frame value (readCell = off + rot + bias) the slot
    // bar is the policy's entry edge — LEFT forward / RIGHT reverse — for EVERY
    // len/rot/off (the R-sweep property, closed-form).
    for (int len : {1, 3, 8, 16, 32}) {
        for (int64_t rot = 0; rot < len; ++rot) {
            for (int64_t off : {0, 7, 31}) {
                // The ANCHOR LAW: the frame value at the entry is rot in EVERY
                // direction (7aae5ee9 — "the anchor lands at R in EVERY direction");
                // the direction's bias lives INSIDE the slot law, which is exactly
                // what puts the playhead on the LEFT edge forward and the RIGHT
                // edge reverse.
                const int64_t entryCell = off + rot;
                const int64_t fwdEdge = off % dotModular::kMaskRing;                        // the LEFT slot's cell
                const int64_t revEdge = (off + len - 1) % dotModular::kMaskRing;            // the RIGHT slot's cell (ring-wrapped)
                const int64_t fwdSlot = PositionResolver::WalkResolver<SequencerEngine::WalkFwd>::playheadSlotBar(entryCell, len, off, rot, dotModular::kMaskRing);
                const int64_t revSlot = PositionResolver::WalkResolver<SequencerEngine::WalkRev>::playheadSlotBar(entryCell, len, off, rot, dotModular::kMaskRing);
                if (fwdSlot != fwdEdge) {
                    std::snprintf(msg, sizeof msg, "ENTRY fwd len=%d rot=%lld off=%lld: slot %lld != LEFT edge %lld",
                                  len, (long long)rot, (long long)off, (long long)fwdSlot, (long long)fwdEdge);
                    CHECK(false, msg);
                }
                if (revSlot != revEdge) {
                    std::snprintf(msg, sizeof msg, "ENTRY rev len=%d rot=%lld off=%lld: slot %lld != RIGHT edge %lld",
                                  len, (long long)rot, (long long)off, (long long)revSlot, (long long)revEdge);
                    CHECK(false, msg);
                }
            }
        }
    }
    std::printf("  entry-edge property (the T1 R-sweep, closed form): LEFT fwd / RIGHT rev, every len/rot/off\n");

    // 5. THE COORDINATE — the signed phrase coordinate vs the shipped inline.
    long coordChecks = 0;
    for (int64_t p = 0; p <= 64; ++p) {
        for (int s : {1, -1}) {
            for (int64_t adv = -9; adv <= 9; ++adv) {
                for (int lenG : {1, 7, 16}) {
                    const int64_t got = PositionResolver::signedPhraseCoord(p, s, adv, lenG);
                    const int64_t want = shipCoord(p, s, adv, lenG);
                    if (got != want) {
                        std::snprintf(msg, sizeof msg, "COORD p=%lld s=%d adv=%lld lenG=%d: %lld != shipped %lld",
                                      (long long)p, s, (long long)adv, lenG, (long long)got, (long long)want);
                        CHECK(false, msg);
                    }
                    ++coordChecks;
                }
            }
        }
    }
    std::printf("  coordinate grid: %ld (p x sign x advanceOffset x lenG) bit-identical\n", coordChecks);

    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
