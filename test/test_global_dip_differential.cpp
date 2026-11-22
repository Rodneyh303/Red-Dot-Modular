// test/test_global_dip_differential.cpp — CC_FIX_GLOBAL_BELOW_VOICE_PHASE_DESYNC
// (the doc's differential, turned into the guard per Rodney's 2026-11-18 ruling:
// the WITHIN-ENGINE residue-free property, not raw A==B under live modulation).
//
// LEG 1 (STATIC — the regression floor): two engines, same seed + identical gate
// stream; B dips global 16->4->16; NO live modulation. The WANDER (advanceOffset_)
// stays A==B — the diag's own metric ("the dip perturbs nothing, advanceOffset_
// included"). The played columns may legitimately differ: the mono transport is
// itself a probabilistic decision reading the global-bounded variation lane, so
// pos_.phrasePos() is a view of a walk that legitimately differs A vs B (the
// doc's own ruling — that is the music, not a defect).
//
// LEG 2 (LIVE — the on-schedule IFF): the doc's live variation-lane offset CV
// rig, in BOTH row flavours. The offsets MAY diverge — the dipped engine
// genuinely played a different window. What must hold: at every step the
// lane-LOR commit fires IFF the engine's OWN live global-anchored re-entry
// holds this step — the committed value equals the pend when the anchor holds
// and a tickHolders pass ran, and is untouched otherwise. No commit at a step
// the live anchor does not sanction — no latched off-grid step-6/7. 2a runs
// Rodney's diag rig verbatim (identity-delegated rows: the anchor is the global
// phrase entry); 2b runs the worked example's rig (Local-East mapped L=8
// frames — the rotation anchor's own law, exercised through the dip).
//
// (LEG 3, the 2026-11-18 abs1 A-table, is PARKED in test/parked_abs1_content_law.cpp: its content law was
// superseded by the 2026-11-21 cycle-consistent abs1 ruling.)
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "PhiloxRng.hpp"
using redDot::PhiloxRng;
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static const int kLanes[7] = {
    SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST,   SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION,
    SequencerEngine::PL_LEGATO };

static PatternInput makeInput() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = 0.6f;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

// mappedRows: the worked example's rig — Local-East rows with their own L=8
// frame (the rotation anchor's own law). Without it the rows are identity-
// delegated (Rodney's diag verbatim): the anchor is the global phrase entry.
static void seed(SequencerEngine& eng, int nVoices, bool mappedRows) {
    PhiloxRng rng; rng.seed64(12345u);
    eng.reset();
    eng.numPolyVoices = nVoices;
    for (int i = 0; i < 16; ++i) {
        eng.pe.melodyRandom[i]=rng.nextFloat(); eng.pe.octaveRandom[i]=rng.nextFloat();
        eng.pe.qmixRandom[i]=rng.nextFloat();   eng.pe.rhythmRandom[i]=rng.nextFloat();
        eng.pe.accentRandom[i]=rng.nextFloat();  eng.pe.variationRandom[i]=rng.nextFloat();
        eng.pe.legatoRandom[i]=rng.nextFloat();
    }
    for (int v = 2; v <= 16; ++v) {
        const auto vs = VoiceResolver::slot(v);
        for (int lane = 0; lane < 7; ++lane)
            for (int i = 0; i < 16; ++i)
                eng.pe.polyRandom(vs, LaneId{kLanes[lane]})[i] = rng.nextFloat();
        eng.voices[vs].restProb   = 0.20f;
        eng.voices[vs].accentProb = 0.25f;
        eng.voices[vs].legatoProb = 0.0f;
        eng.voices[vs].qmixLevel  = 0.5f;
        eng.voices[vs].variationProb = (v == 2) ? 0.0f : 0.5f;   // V2 centre control
        eng.voiceLorRef(vs, SequencerEngine::LOR_LEN) = 8;       // the voice frame (engages under mappedRows)
        eng.polyLORRef(vs, LaneId{SequencerEngine::PL_VARIATION}, SequencerEngine::LOR_LEN) = 8;
        if (mappedRows) eng.setVoiceLorLocalEast(vs, true);
    }
}

static void stepB(SequencerEngine& eng, const PatternInput& in, bool rose, bool high) {
    StepResult r = eng.executeModeB(rose, high, 6.f, in);
    if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
}

static int wrapModL(int64_t v, int m) { int64_t r = v % m; if (r < 0) r += m; return (int)r; }

// The engine's voiceFrameEdgeLive_ law, mirrored from the public state: the
// identity row rides the global phrase edge; a mapped row keys off the
// transport-signed phrase coordinate (+ the voice's advance offset, mod the
// CURRENT global length) landing the walk's start (Forward rows: mod L).
static bool anchoredEdge(const SequencerEngine& eng, int slot) {
    int L, O, R; SequencerEngine::LaneDir d;
    eng.effectiveVoiceLor(slot, L, O, R, d);
    if (L == dotModular::kPatternLengthCap && O == 0 && R == 0 && d == SequencerEngine::LaneDir::Forward)
        return eng.pos_.phrasePos() == 0;   // the identity row: the global phrase entry
    if (L < 1) L = 1;
    const int lenG = eng.cachedLength > 0 ? eng.cachedLength : 1;
    const int64_t sPosV = (eng.pos_.direction() > 0) ? (int64_t)eng.pos_.phrasePos()
                                                     : -(int64_t)eng.pos_.phrasePos();
    return wrapModL(wrapModL(sPosV + eng.advanceOffset_[slot], lenG), L) == 0;
}

// ── LEG 1 + LEG 2: the differential march ─────────────────────────────────────
static int marchDifferential(bool liveMod, bool mappedRows, int STEPS, int DIP_ON, int DIP_OFF, uint64_t gateSeed = 999u, int* maxPhraseOut = nullptr) {
    const int N = 4;   // V2..V5
    SequencerEngine A, B; seed(A, N, mappedRows); seed(B, N, mappedRows);
    A.updateWindow(16.f,0.f,false,0.f,0.f,false);
    B.updateWindow(16.f,0.f,false,0.f,0.f,false);
    const PatternInput in = makeInput();

    PhiloxRng grng; grng.seed64(gateSeed);
    bool high = false;
    int maxPhrase = 0;
    int firstOffsetDiverge = -1;
    int postRestoreCommits = 0;
    const int el = SequencerEngine::editorLane(SequencerEngine::PL_VARIATION);
    std::vector<int> prevCommitted(kVoiceSlots, 0);
    std::vector<int> prevOffsets(kVoiceSlots, 0);
    for (int vn = 2; vn <= 1 + N; ++vn)
        prevCommitted[VoiceResolver::slot(vn).v] = B.laneLorCv_[VoiceResolver::slot(vn).v][el][SequencerEngine::LOR_OFF];
    for (int i = 0; i < kVoiceSlots; ++i) prevOffsets[i] = (int)B.advanceOffset_[i];
    int64_t prevTick = B.transportTick_;

    char msg[256];
    for (int s = 0; s < STEPS; ++s) {
        bool rose = false;
        if (high) { if (grng.nextFloat() < 0.40f) high = false; }
        else      { if (grng.nextFloat() < 0.55f) { high = true; rose = true; } }

        const int varOff = (int)std::lround(3.0 * std::sin(s * 0.5));   // -3..+3, swinging
        if (liveMod) {
            for (int vn = 3; vn <= 1 + N; ++vn) {           // the WANDERING voices V3..V5
                int d3[3] = {0, varOff, 0};
                A.pushLaneLorCvPoly(VoiceResolver::slot(vn), SequencerEngine::PL_VARIATION, d3);
                B.pushLaneLorCvPoly(VoiceResolver::slot(vn), SequencerEngine::PL_VARIATION, d3);
            }
        }

        if (s == DIP_ON)  B.updateWindow(4.f,0.f,false,0.f,0.f,false);
        if (s == DIP_OFF) B.updateWindow(16.f,0.f,false,0.f,0.f,false);

        stepB(A, in, rose, high);
        stepB(B, in, rose, high);

        // LEG 1's floor: the WANDER (the diag's own metric) stays A==B — the
        // columns/reads may legitimately differ (the mono's own wander).
        bool offsetsDiverged = false;
        for (int i = 1; i <= N; ++i)
            if (A.advanceOffset_[i] != B.advanceOffset_[i]) offsetsDiverged = true;
        if (offsetsDiverged && firstOffsetDiverge < 0) firstOffsetDiverge = s;
        if (offsetsDiverged && !liveMod) {
            std::snprintf(msg, sizeof msg, "LEG1(static%s): the WANDER diverged at step %d — the dip with NOTHING swinging perturbed advanceOffset_ (residue)", mappedRows ? "-mapped" : "", s);
            CHECK(false, msg);
            return firstOffsetDiverge;
        }

        // LEG 2's IFF: the commit can only run when a tickHolders pass ran — an
        // advance (the transport tick moved) or an offsets-only recompute (a
        // diverged voice moved on a held rise). A held rise with nobody moving
        // runs NO head: no commit is possible however the anchor sits.
        const bool tickMoved = (B.transportTick_ != prevTick);
        bool offsetsChanged  = false;
        for (int i = 1; i <= N; ++i) if ((int)B.advanceOffset_[i] != prevOffsets[i]) offsetsChanged = true;
        const bool headRan = tickMoved || offsetsChanged;
        if (liveMod) {
            for (int vn = 3; vn <= 1 + N; ++vn) {
                const int slot = VoiceResolver::slot(vn).v;
                const int now = B.laneLorCv_[slot][el][SequencerEngine::LOR_OFF];
                const bool anchor = anchoredEdge(B, slot);
                const int expected = (headRan && anchor) ? varOff : prevCommitted[slot];
                if (now != expected) {
                    int rL, rO, rR; SequencerEngine::LaneDir rD;
                    B.effectiveVoiceLor(slot, rL, rO, rR, rD);
                    std::snprintf(msg, sizeof msg,
                                  "LEG2(live%s): V%d's lane CV at step %d is %d, want %d (headRan=%d, anchor=%d, phrasePos=%d, offset=%+d, lenG=%d, pend=%d, rowL=%d) — %s",
                                  mappedRows ? "-mapped" : "", vn + 1, s, now, expected, (int)headRan, (int)anchor,
                                  B.pos_.phrasePos(), (int)B.advanceOffset_[slot], B.cachedLength,
                                  B.laneLorCvPend_[slot][el][SequencerEngine::LOR_OFF], rL,
                                  (headRan && anchor)
                                      ? "the anchored re-entry must land the pend"
                                      : "no commit off the live anchor (the latched magic step 6/7)");
                    CHECK(false, msg);
                }
                if (headRan && anchor && s >= DIP_OFF) ++postRestoreCommits;   // the cadence is alive (the landing is the IFF above)
                prevCommitted[slot] = now;
            }
        }
        if (s >= DIP_OFF) maxPhrase = std::max(maxPhrase, (int)B.pos_.phrasePos());
        prevTick = B.transportTick_;
        for (int i = 0; i < kVoiceSlots; ++i) prevOffsets[i] = (int)B.advanceOffset_[i];
    }
    if (liveMod && mappedRows) {
        // The cadence-alive check runs on the MAPPED leg (the rotation anchor's own
        // law — crossings twice per phrase + the offset shifts); the identity leg's
        // anchor fires once per phrase and a given gate stream can legitimately
        // hold through the post-restore crossings (the IFF above still validates
        // every commit it observes).
        std::snprintf(msg, sizeof msg, "LEG2(live-mapped): no anchored firing observed post-restore in %d steps — the cadence check is vacuous", STEPS);
        CHECK(postRestoreCommits > 0, msg);
    }
    if (maxPhraseOut) *maxPhraseOut = maxPhrase;
    return firstOffsetDiverge;
}

int main() {
    std::printf("test_global_dip_differential — CC_FIX_GLOBAL_BELOW_VOICE_PHASE_DESYNC (the residue-free guard)\n");
    {   // LEG 1: static — the dip perturbs nothing (the WANDER stays A==B).
        const int d = marchDifferential(/*liveMod=*/false, /*mappedRows=*/false, 52, 16, 26);
        std::printf("LEG1 (static, identity rows): the wander %s\n", d < 0 ? "A==B throughout — no residue" : "DIVERGED");
        CHECK(d < 0, "LEG1(static): the static differential's wander must stay identical through the dip");
    }
    // The IFF is only as strong as the phrase positions the march VISITS (a spurious commit at a position the
    // transport never reaches is invisible: found by mutation 2026-11-22 — an extra commit at phrasePos 5 passed
    // because a 52-step run never got past 3). So: several gate streams, a long run, and a COVERAGE floor.
    const uint64_t seeds[] = {999u, 7u, 31u, 2024u, 4242u};
    for (int mapped = 0; mapped <= 1; ++mapped) {
        int reach = 0;
        for (uint64_t sd : seeds) {
            int mp = 0;
            const int d = marchDifferential(/*liveMod=*/true, /*mappedRows=*/mapped != 0, 240, 16, 26, sd, &mp);
            reach = std::max(reach, mp);
            std::printf("LEG2%c (live, %s rows) seed %llu: wander first diverged at step %d (legitimate); max phrase pos after restore %d\n",
                        mapped ? 'b' : 'a', mapped ? "mapped L=8" : "identity", (unsigned long long)sd, d, mp);
        }
        char msg[160];
        std::snprintf(msg, sizeof msg, "LEG2%c coverage: the marches reached phrase position %d only — the commit IFF is vacuous above it (want >= 7)", mapped ? 'b' : 'a', reach);
        CHECK(reach >= 7, msg);
    }
    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
