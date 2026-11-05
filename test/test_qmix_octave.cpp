// test_qmix_octave.cpp — QMIX_OCTAVE Stage 1: the split of the single Qmix crossfade into
// TWO independent reads of the SAME r_qmix stream (degree + octave), with the octave
// threshold following the degree threshold by default (CC_PROMPT_QMIX_OCTAVE +
// QMIX_OCTAVE_PLAN.md).
//
// Covered (the spec's GATE: the split with no new control is the proof):
//   1. genPitchSplit / genPitchLive BIT-IDENTITY — genPitchLive re-expressed on the split
//      helper keeps the frozen legacy 12-TET + Micro formulas bit-exact (raw-float compare,
//      the test_12tet_identity discipline) across a swept (r_semi, r_oct, transpose, octave
//      range) grid; the components reproduce the formula-grouped terms (12-TET transpose
//      rides the DEGREE term, Micro's octaves-only transpose rides the OCTAVE term).
//   2. voicePitch TRUTH TABLE at a quantiser mode (deterministic probes, hand-computed):
//      both-external == the external branch verbatim; both-generated == genPitchLive
//      verbatim; the MIXED rows compose each source's contribution by its own grouping —
//      gen-deg/ext-oct = floor(vExt) + DEG_GEN (transpose INCLUDED, the 12-TET grouping),
//      ext-deg/gen-oct = OCT_GEN + frac(vExt) (12-TET transpose ABSENT — it belongs to the
//      generated degree term; Micro shiftTerm INCLUDED, the octaves-only ruling) — the
//      musical case: the external theme's degree rides the voice's own generated octave.
//      outSem names the DEGREE source (the markSemi law); mixed rows clamp [-5,+5].
//   3. THE ENGINE DEFAULT LAW (adjudication 3, the subResLive precedent): qmixOctResolved()
//      == qmixLevel bit-exact while qmixOctLive is false — so the octave decision IS the
//      degree decision on every engine nobody resolved (all existing tests, hand-set
//      qmixLevel marches included) — bit-identity by construction; flipping the gate
//      (Stage 2's controller) switches to the stored level verbatim.
//   4. FAILED PICK mirrors: all-zero weights -> genPitchLive 0.f / outSem 0 exactly as
//      frozen; the deg=gen mixed row mirrors the 0.f; the deg=ext mixed row still composes
//      the generated octave (the axes are independent reads — the roll happens regardless).
//   5. Quantiser-mode gating: outside a quantiser mode ALL FOUR rows fall through to
//      genPitchLive unchanged (quantiserPitchSource gates the whole table).
//   6. CA routing: the external operand (both-external AND mixed rows) reads the CA-routed
//      quantiserCV row (caInputCvSrcRow), identity default — the QMIX_LANE_PARITY blend law.
//
// Compile: see test/run_all.sh (links $SE $GS $PE — the REAL engines, the
// test_12tet_identity harness shape).
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "SequencerEngine.hpp"
#include "../src/dsp/VoiceResolver.hpp"   // the VoiceSlot minter (mint-only enforcement)
#include <iostream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cmath>

// ---- test framework (repo convention) ----
static int s_pass = 0, s_fail = 0;
#define SUITE(n) do{std::cout<<"\n\033["<<(n)<<"]\033\n";}while(0)
#define TEST(desc,...) do{ \
    bool _ok=true; std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033PASS\033 "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033FAIL\033 "<<(desc); \
         if(!_msg.empty())std::cout<<" -- "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_BITEQ(a,b) do{ float _x=(a), _y=(b); \
    if(std::memcmp(&_x,&_y,sizeof(float))!=0){ std::ostringstream _s; \
      _s<<#a<<"="<<_x<<" ("<<std::hexfloat<<_x<<") not bit-equal "<<#b<<"="<<_y<<" ("<<_y<<")"; \
      throw std::runtime_error(_s.str()); } }while(0)

// ── The FROZEN legacy formulas — the reference the split must reproduce bit-exactly ──────────
static int legacyPick(const float w[], int n, float r_val) {
    float sum = 0.f; for (int i = 0; i < n; ++i) sum += w[i];
    if (sum <= 0.0001f) return -1;
    float r = std::clamp(r_val, 0.f, 1.f - 1e-7f) * sum;
    float acc = 0.f;
    for (int i = 0; i < n; ++i) {
        if (w[i] <= 0.f) continue;
        acc += w[i];
        if (r < acc) return i;
    }
    return n - 1;
}
static float legacyGenPitch(int& outSem, const PatternInput& in,
                            const dotModular::TuningTable& tuning, float r_semi, float r_oct) {
    int sem = legacyPick(in.semiWeights, tuning.N, r_semi);
    outSem = (sem < 0) ? 0 : sem;
    if (sem < 0) return 0.f;
    float lo = in.octaveLo, hi = in.octaveHi;
    if (hi < lo) std::swap(lo, hi);
    int oL = (int)std::floor(lo);
    int oH = (int)std::floor(hi);
    float roll = std::clamp(r_oct, 0.f, 0.9999f);
    int oct = oL + (int)std::floor(roll * float(oH - oL + 1));
    float v;
    if (tuning.isDefault12TET) {
        v = float(oct) - 4.f + (sem + in.transpose) / 12.f;
    } else {
        const float octShift = std::round(in.transpose / 12.f);
        v = float(oct) - 4.f + tuning.degreeVolts(sem) + octShift;
    }
    return std::clamp(v, -5.f, 5.f);
}

static PatternInput makeInput(float lo, float hi, float transpose) {
    PatternInput in{};
    for (int i = 0; i < dotModular::TuningTable::MAXN; ++i) in.semiWeights[i] = 1.f;
    in.octaveLo = lo; in.octaveHi = hi; in.transpose = transpose;
    return in;
}

int main() {
    std::cout << "QMIX_OCTAVE Stage 1 — the split (two reads of one stream)\n";
    SequencerEngine eng;

    // ══ 1. genPitchSplit / genPitchLive BIT-IDENTITY ══════════════════════════════════════════
    SUITE("1. genPitchLive on genPitchSplit == the frozen legacy formulas (bit-exact)");
    {
        // 12-TET sweep: octave ranges (incl. the reversed pair), transposes, r grid.
        const float los[4]  = { -2.f, 0.f, 1.f, 3.f };
        const float his[4]  = { 5.f, 2.f, 4.f, -1.f };   // last pair reversed (hi < lo)
        const float trs[5]  = { -12.f, -5.f, 0.f, 7.f, 12.f };
        for (int oi = 0; oi < 4; ++oi) for (int ti = 0; ti < 5; ++ti) {
            PatternInput in = makeInput(los[oi], his[oi], trs[ti]);
            for (int a = 0; a <= 10; ++a) for (int b = 0; b <= 10; ++b) {
                const float rs = a / 10.f, ro = b / 10.f;
                int semA = -99, semB = -99;
                const float vA = eng.pe.genPitchLive(semA, in, rs, ro);
                const float vB = legacyGenPitch(semB, in, eng.pe.tuning, rs, ro);
                if (std::memcmp(&vA, &vB, sizeof(float)) != 0 || semA != semB) {
                    std::ostringstream m;
                    m << "12-TET diverged lo=" << los[oi] << " hi=" << his[oi]
                      << " tr=" << trs[ti] << " rs=" << rs << " ro=" << ro
                      << " vA=" << vA << " vB=" << vB << " semA=" << semA << " semB=" << semB;
                    throw std::runtime_error(m.str());
                }
                // The split's components reassemble to the SAME bits, association-exact.
                const auto s = eng.pe.genPitchSplit(in, rs, ro);
                const float vS = std::clamp((s.octTerm + s.degTerm) + s.shiftTerm, -5.f, 5.f);
                if (std::memcmp(&vS, &vB, sizeof(float)) != 0) {
                    std::ostringstream m;
                    m << "split reassembly diverged rs=" << rs << " ro=" << ro
                      << " vS=" << vS << " vB=" << vB;
                    throw std::runtime_error(m.str());
                }
                EXPECT(s.sem == semB);
                EXPECT(s.picked == (legacyPick(in.semiWeights, eng.pe.tuning.N, rs) >= 0));
            }
        }
        TEST("12-TET sweep: 4 octave ranges x 5 transposes x 121 (r_semi, r_oct) pairs bit-exact", {});

        // Micro sweep: a non-default cents table (one degree detuned) flips the formula.
        eng.pe.tuning.resetToEqual12TET();
        eng.pe.tuning.cents[1] = 150.f;
        eng.pe.tuning.cents[5] = 275.f;
        eng.pe.tuning.recomputeDefaultFlag();
        TEST("micro table flips isDefault12TET", EXPECT(!eng.pe.tuning.isDefault12TET));
        for (int oi = 0; oi < 4; ++oi) for (int ti = 0; ti < 5; ++ti) {
            PatternInput in = makeInput(los[oi], his[oi], trs[ti]);
            for (int a = 0; a <= 10; ++a) for (int b = 0; b <= 10; ++b) {
                const float rs = a / 10.f, ro = b / 10.f;
                int semA = -99, semB = -99;
                const float vA = eng.pe.genPitchLive(semA, in, rs, ro);
                const float vB = legacyGenPitch(semB, in, eng.pe.tuning, rs, ro);
                if (std::memcmp(&vA, &vB, sizeof(float)) != 0 || semA != semB) {
                    std::ostringstream m;
                    m << "micro diverged lo=" << los[oi] << " hi=" << his[oi]
                      << " tr=" << trs[ti] << " rs=" << rs << " ro=" << ro;
                    throw std::runtime_error(m.str());
                }
                const auto s = eng.pe.genPitchSplit(in, rs, ro);
                const float vS = std::clamp((s.octTerm + s.degTerm) + s.shiftTerm, -5.f, 5.f);
                if (std::memcmp(&vS, &vB, sizeof(float)) != 0) {
                    std::ostringstream m;
                    m << "micro split reassembly diverged rs=" << rs << " ro=" << ro;
                    throw std::runtime_error(m.str());
                }
            }
        }
        TEST("micro sweep: same grid bit-exact through the cents path", {});

        // The formula-grouped placement of transpose, pinned WHERE IT LIVES:
        // 12-TET -> degTerm; Micro -> shiftTerm. (adjudication 2)
        {
            eng.pe.tuning.resetToEqual12TET();
            PatternInput in = makeInput(0.f, 2.f, 12.f);
            const auto s12 = eng.pe.genPitchSplit(in, 0.05f, 0.0f);   // sem 0 (sum 12, r .6 -> deg 0? recompute below)
            // r_semi 0.05 -> r = 0.6 -> walk crosses at degree 0 (acc 1 > 0.6). sem 0.
            EXPECT(s12.sem == 0);
            EXPECT(s12.shiftTerm == 0.f);
            EXPECT_BITEQ(s12.degTerm, (0.f + 12.f) / 12.f);
            TEST("12-TET transpose rides the DEGREE term", {});
            eng.pe.tuning.cents[1] = 150.f; eng.pe.tuning.recomputeDefaultFlag();
            const auto sMi = eng.pe.genPitchSplit(in, 0.05f, 0.0f);
            EXPECT(sMi.shiftTerm == 1.f);   // round(12/12)
            EXPECT_BITEQ(sMi.degTerm, eng.pe.tuning.degreeVolts(0));
            TEST("micro transpose rides the OCTAVE term (shiftTerm)", {});
            eng.pe.tuning.resetToEqual12TET();
        }
    }

    // ══ 2. THE ENGINE DEFAULT LAW ═════════════════════════════════════════════════════════════
    SUITE("2. qmixOctResolved() == qmixLevel while the gate is closed (bit-exact)");
    {
        auto& v = eng.voices[dotModular::VoiceResolver::mono()];
        v.qmixLevel = 0.0f; v.qmixOctLevel = 0.0f; v.qmixOctLive = false;
        TEST("fresh voice: resolved == level == 0", EXPECT_BITEQ(v.qmixOctResolved(), v.qmixLevel));
        v.qmixLevel = 0.7f; v.qmixOctLevel = 0.123f;   // a STALE stored level must NOT leak
        TEST("stale stored level ignored while gate closed",
             EXPECT_BITEQ(v.qmixOctResolved(), 0.7f));
        v.qmixOctLive = true;
        TEST("gate open: the stored level drives verbatim",
             EXPECT_BITEQ(v.qmixOctResolved(), 0.123f));
        v.qmixOctLive = false; v.qmixLevel = 0.0f; v.qmixOctLevel = 0.0f;
        // The DECISION-EQUALITY property: with the gate closed the octave comparison IS the
        // degree comparison on the same draw — sweep r x level, both bools agree everywhere.
        bool allEq = true;
        for (int a = 0; a <= 20 && allEq; ++a) for (int b = 0; b <= 20 && allEq; ++b) {
            const float r = a / 20.f, L = b / 20.f;
            v.qmixLevel = L;
            const bool deg = (r >= v.qmixLevel), oct = (r >= v.qmixOctResolved());
            allEq &= (deg == oct);
        }
        v.qmixLevel = 0.0f;
        TEST("gate closed: (r >= resolved) == (r >= qmixLevel) over the r x level grid", EXPECT(allEq));
    }

    // ══ 3. voicePitch TRUTH TABLE (quantiser mode, deterministic, hand-computed) ══════════════
    SUITE("3. voicePitch: two reads of one stream — the four rows");
    {
        // Deterministic setup: 12-TET, quantiser live, one active-chord quantise list,
        // identity CA routing (default), CV 2.5 -> quantises to 2 + 7/12 (degree 7 of {0,4,7}).
        eng.pe.tuning.resetToEqual12TET();
        eng.quantiserPitchSource = true;
        // caMelodySrc defaults to the identity diagonal (the CA routing default) — no setup.
        for (int i = 0; i < 16; ++i) eng.quantiserCV[i] = 0.f;
        eng.quantiserCV[0] = 2.5f;
        eng.activeSemiCount = 3;
        eng.activeSemiList[0] = 0; eng.activeSemiWeight[0] = 1.f;
        eng.activeSemiList[1] = 4; eng.activeSemiWeight[1] = 1.f;
        eng.activeSemiList[2] = 7; eng.activeSemiWeight[2] = 1.f;

        PatternInput in = makeInput(0.f, 1.f, 0.f);
        const float vExtWant = 2.f + 7.f / 12.f;   // quantise(2.5) over {0,4,7} within oct 2
        TEST("deterministic probes: vExt quantise + the split components", {
            const float vExt = eng.quantize(2.5f);
            EXPECT_BITEQ(vExt, vExtWant);
            const auto split = eng.pe.genPitchSplit(in, 0.3f, 0.25f);   // sem 3, oct 0 -> octTerm -4
            EXPECT(split.picked && split.sem == 3);
            EXPECT_BITEQ(split.octTerm, -4.f);
            EXPECT_BITEQ(split.degTerm, 3.f / 12.f);
            EXPECT(split.shiftTerm == 0.f);
        });

        const auto slot0 = dotModular::VoiceResolver::mono();          // V1/mono, slot 0
        const auto slot2 = dotModular::VoiceResolver::slot(2);         // V2, slot 1

        // Row BOTH-EXTERNAL — the external branch verbatim (bit-identical).
        {
            int sem = -99;
            const float v = eng.voicePitch(slot0, sem, in, 0.3f, 0.25f, false, false);
            EXPECT_BITEQ(v, vExtWant);
            EXPECT(sem == 7);
            TEST("deg ext + oct ext == the external branch verbatim (v, outSem)", {});
        }
        // Row BOTH-GENERATED — genPitchLive verbatim (bit-identical).
        {
            int sem = -99, semRef = -99;
            const float v     = eng.voicePitch(slot0, sem, in, 0.3f, 0.25f, true, true);
            const float vRef  = eng.pe.genPitchLive(semRef, in, 0.3f, 0.25f);
            EXPECT_BITEQ(v, vRef);
            EXPECT(sem == semRef);
            EXPECT_BITEQ(v, -3.75f);   // (0-4) + 3/12
            TEST("deg gen + oct gen == genPitchLive verbatim (v=-3.75, outSem=3)", {});
        }
        // MIXED deg GEN, oct EXT — the generated degree seated in the external octave.
        {
            int sem = -99;
            const float v = eng.voicePitch(slot0, sem, in, 0.3f, 0.25f, true, false);
            EXPECT_BITEQ(v, 2.f + 3.f / 12.f);   // floor(vExt)=2 + degTerm 0.25
            EXPECT(sem == 3);                     // outSem names the DEGREE source
            TEST("deg gen + oct ext: floor(vExt)+DEG_GEN = 2.25, outSem=3", {});
        }
        // MIXED deg EXT, oct GEN — THE MUSICAL CASE: the external degree rides the generated octave.
        {
            int sem = -99;
            const float v = eng.voicePitch(slot0, sem, in, 0.3f, 0.25f, false, true);
            const float frac = vExtWant - std::floor(vExtWant);
            EXPECT_BITEQ(v, (-4.f + 0.f) + frac);   // octTerm(-4) + shiftTerm(0) + frac
            EXPECT(sem == 7);                        // outSem names the DEGREE source (external)
            TEST("deg ext + oct gen: OCT_GEN+frac = -3.4166..., outSem=7 (the musical case)", {});
        }

        // The 12-TET transpose placement in the MIXED rows (adjudication 2):
        // rides the generated DEGREE (deg=gen row), ABSENT from the generated OCTAVE
        // (deg=ext row) — the formula's own grouping.
        {
            PatternInput inT = makeInput(0.f, 1.f, 12.f);
            int sem = -99;
            const float vGen = eng.voicePitch(slot0, sem, inT, 0.3f, 0.25f, true, false);
            EXPECT_BITEQ(vGen, 2.f + (3.f + 12.f) / 12.f);   // transpose INCLUDED
            EXPECT(sem == 3);
            const float vOct = eng.voicePitch(slot0, sem, inT, 0.3f, 0.25f, false, true);
            const float frac = vExtWant - std::floor(vExtWant);
            EXPECT_BITEQ(vOct, (-4.f + 0.f) + frac);          // transpose ABSENT
            TEST("12-TET transpose: rides deg=gen, absent from deg=ext (formula grouping)", {});
        }

        // Mixed rows clamp [-5,+5] (new paths; safe default).
        {
            PatternInput inHi = makeInput(0.f, 1.f, 12.f);
            inHi.semiWeights[11] = 1.f;   // uniform already; sem 11 via r_semi ~ 1
            int sem = -99;
            eng.quantiserCV[0] = 5.2f;    // quantises to 5.0 (degree 0 at octave 5)
            const float vHi = eng.voicePitch(slot0, sem, inHi, 0.9999f, 0.25f, true, false);
            EXPECT_BITEQ(vHi, 5.f);       // 5 + (11+12)/12 = 6.9166 -> clamped
            eng.quantiserCV[0] = 2.5f;
            PatternInput inLo = makeInput(-2.f, -2.f, 0.f);
            const float vLo = eng.voicePitch(slot0, sem, inLo, 0.3f, 0.25f, false, true);
            const float frac = vExtWant - std::floor(vExtWant);
            EXPECT_BITEQ(vLo, std::clamp((-6.f + 0.f) + frac, -5.f, 5.f));   // -5.4166 -> -5
            TEST("mixed rows clamp [-5,+5]", {});
        }

        // Failed pick: genPitchLive's 0.f result mirrored in the deg=gen mixed row; the
        // deg=ext row still composes the generated octave (independent axes).
        {
            PatternInput inZ = makeInput(0.f, 1.f, 0.f);
            for (int i = 0; i < dotModular::TuningTable::MAXN; ++i) inZ.semiWeights[i] = 0.f;
            int sem = -99, semRef = -99;
            EXPECT_BITEQ(eng.pe.genPitchLive(semRef, inZ, 0.3f, 0.25f), 0.f);
            EXPECT(semRef == 0);
            const float vGen = eng.voicePitch(slot0, sem, inZ, 0.3f, 0.25f, true, false);
            EXPECT_BITEQ(vGen, 0.f); EXPECT(sem == 0);
            const float vOct = eng.voicePitch(slot0, sem, inZ, 0.3f, 0.25f, false, true);
            const float frac = vExtWant - std::floor(vExtWant);
            EXPECT_BITEQ(vOct, (-4.f + 0.f) + frac);   // the roll still happened
            TEST("failed pick: deg=gen mirrors 0.f; deg=ext still rolls the octave", {});
        }

        // Quantiser gating: outside a quantiser mode ALL FOUR rows fall through to
        // genPitchLive unchanged.
        {
            eng.quantiserPitchSource = false;
            const bool rows[4][2] = {{false,false},{true,true},{true,false},{false,true}};
            for (auto& r : rows) {
                int sem = -99, semRef = -99;
                const float v = eng.voicePitch(slot0, sem, in, 0.3f, 0.25f, r[0], r[1]);
                const float vRef = eng.pe.genPitchLive(semRef, in, 0.3f, 0.25f);
                EXPECT_BITEQ(v, vRef); EXPECT(sem == semRef);
            }
            eng.quantiserPitchSource = true;
            TEST("quantiserPitchSource=false: all four rows == genPitchLive", {});
        }

        // CA routing: the external operand reads the CA-routed CV row (identity default ->
        // own channel; a rerouted row reads THAT voice's line — the QMIX_LANE_PARITY law).
        // numPolyVoicesHint gates the CA row's OOB=identity guard — a reroute only engages
        // when the source voice is within the active count, so raise it for this probe.
        {
            for (int i = 0; i < 16; ++i) eng.quantiserCV[i] = 0.f;
            eng.quantiserCV[5] = 2.5f;
            eng.pe.numPolyVoicesHint = 8;   // the CA row's active-count gate
            // Route voice 1's input-CV operand to source voice 5 via the CA melody plane.
            eng.pe.caMelodySrc[1] = 5;
            int sem = -99;
            const float v = eng.voicePitch(slot2, sem, in, 0.3f, 0.25f, false, false);
            EXPECT_BITEQ(v, vExtWant);   // the routed line's quantised voltage
            const float vMix = eng.voicePitch(slot2, sem, in, 0.3f, 0.25f, false, true);
            const float frac = vExtWant - std::floor(vExtWant);
            EXPECT_BITEQ(vMix, (-4.f + 0.f) + frac);   // the MIXED row rides the SAME routing
            eng.pe.caMelodySrc[1] = 1;
            eng.pe.numPolyVoicesHint = 0;
            for (int i = 0; i < 16; ++i) eng.quantiserCV[i] = 0.f;
            eng.quantiserCV[0] = 2.5f;
            TEST("external operand CA-routed (identity default + a rerouted row, both rows)", {});
        }
    }

    // ══ 4. POLY VOICE: the same table through the poly slot (the v.qmixOctResolved() read) ════
    SUITE("4. the poly slot: per-voice octave threshold, same truth table");
    {
        auto& v = eng.voices[dotModular::VoiceResolver::slot(2)];   // V2's poly slot
        v.qmixLevel = 0.0f; v.qmixOctLevel = 0.0f; v.qmixOctLive = false;
        PatternInput in = makeInput(0.f, 1.f, 0.f);
        // Gate closed: the poly decision pair is ONE decision (bit-identical caller path).
        EXPECT_BITEQ(v.qmixOctResolved(), 0.0f);
        // Gate open + divergent level: the mixed rows are reachable per voice.
        v.qmixLevel = 1.0f;          // degree always generated
        v.qmixOctLevel = 0.0f; v.qmixOctLive = true;   // octave always external
        const float vExtWant = 2.f + 7.f / 12.f;
        eng.quantiserCV[1] = 2.5f;
        int sem = -99;
        // r_qmix 0.5, polarity §29 (r >= level -> GENERATED): 0.5 >= 1.0 false -> external
        // degree; 0.5 >= 0.0 true -> generated octave. The (ext, gen) row — the musical case.
        const float got = eng.voicePitch(dotModular::VoiceResolver::slot(2), sem, in, 0.3f, 0.25f,
                                         /*deg:*/(0.5f >= v.qmixLevel),
                                         /*oct:*/(0.5f >= v.qmixOctResolved()));
        const float frac = vExtWant - std::floor(vExtWant);
        EXPECT_BITEQ(got, (-4.f + 0.f) + frac);
        EXPECT(sem == 7);
        TEST("poly slot: divergent levels -> the musical case per voice", {});
        v.qmixLevel = 0.0f; v.qmixOctLevel = 0.0f; v.qmixOctLive = false;
        eng.quantiserCV[1] = 0.f;
    }

    eng.quantiserPitchSource = false;

    std::cout << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail ? 1 : 0;
}
