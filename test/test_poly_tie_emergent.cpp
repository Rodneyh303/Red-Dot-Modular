// test_poly_tie_emergent.cpp — CC_PROMPT_POLY_TIE_EMERGENT: TIE IS EMERGENT.
// One rule at every join: a tie = the same degree AND the same octave (the same
// drawn pitch, within kPitchEps); anything else is a slide (Legato). The mono join
// (~:2037) compared the DEGREE only, so a same-degree/different-octave draw was
// classed Tie and the new octave was silently discarded (the old extendHold never touched
// currentPitchV). Poly S1 froze on MONO's tie (connect && isTie) and the S1/S2
// slide arms never compared pitch at all — a held quantiser CV produced a
// zero-interval "Legato". Sites under test (post-change):
//   MONO join, POLY S1 (the landing connect), POLY S2 (mid-hold, merged arm),
//   the ONSET-path slide arm (:2838 — Rodney's ruling: the same derive).
//
// THE RIG: the WORKING poly march harness (test_step_advance's, via
// test_poly_legato_quantiser): the star graph, seedEngine, GateStream,
// forestDrive. Quantiser mode, all-quantised (the CV is the pitch authority) so
// every draw is CONTROLLABLE from the schedule. The stream is DENSE (a rise
// every ~2.4 steps) so joins are plentiful.
//
// THE INDEPENDENT ORACLE: the SCHEDULE. Every channel sits exactly at a degree-1
// voltage (octave + 100/1200); the expected verdict at a join is computed from
// the schedule's octaves (same degree always, octave equality) — never from the
// engine's isSamePitch, never from quantize(). The held side is the engine's own
// pre-edge state (observed); the new side is the schedule (authored).
//
// THE RIG TRAP THIS SUITE HAD TO FIX (the probe run): a headless engine never
// ingests the scale (the module layer's rebuildSemiCache_ is what calls
// rebuildScaleCache), so quantize() takes the COLD path — which returns the
// clamped input but NEVER WRITES lastQuantOut. The one-entry memo then serves
// reset's 0.0 to every repeated input. The quantiser suite never saw it (its
// distinct per-voice channels thrash the memo — every call misses); THIS suite's
// constant-per-window channels tripped it: every draw came back 0.0. The march
// ingests the all-on scale below so the warm path (which sets the memo) runs.
// (Latent engine note for the report: the cold path leaving lastQuantOut stale
// is a real memo bug if any mode ever runs cold with repeated inputs.)
//
// THE MARCHES:
//   M1 mono: octaves 1→2→1 at fixed steps — the mono octave bug (case 1) + the
//      mono same-pitch tie (case 2) + the tie label (lastNoteType) + the quant
//      flash. RED today: the octave join Ties and keeps the old octave.
//   M2 poly matrix: mono ties/slides on its own schedule; even/odd voices flip
//      octaves at DIFFERENT steps — poly independence both directions (case 3),
//      poly octave (4), per-voice 16ch CV (6), the S1 tie/slide/re-articulate
//      counters, the quant flash at the voice tie (the isQuant gap at today's
//      freeze arm).
//   M3 epsilon (NOT red on the old engines — the mutation-(d) catcher): a
//      microtuning retune (degree 1 +0.01 cents = 8.3e-6 V) mid-march; the same
//      degree drawn across the retune must STILL tie (|ΔV| < kPitchEps). Red
//      only under the epsilon-0 mutant. The CV is nudged +5e-4 at the retune so
//      the quantize memo recomputes against the new table.
//   M4 LegatoMax: mono legatoProb 1.0 (every played step slideMaxes — the forced
//      slide, UNTOUCHED by the change), tieAcrossRests off (real dead windows).
//      The held mono edges land at S1: the voices' same-pitch connects must TIE
//      (before the tie rule they slid to the same pitch = the zero-interval Legato). The
//      from-dead edges are the onset-arm precondition — counted; see below.
//
// THE STRUCTURAL ZEROS (counted, printed, NOT asserted > 0 — Rodney's ruling
// 2026-11-22, honest counters):
//   s2Tie/s2Slide — S2's Tie/Legato arms test the SAME const rd.gateType that
//      monoConnectLanding tested to route here, so inside the mid-hold branch
//      gateType can never be Legato/LegatoMax/Tie. The merged arm is kept
//      uniform per the ruling; it cannot fire.
//   onsetArm — the onset-path slide arm needs a container that legatoed FROM
//      DEAD while the child still sounds. The only from-dead legato producer is
//      mono's slideMax, and the drive closes every mono-descended child one edge
//      after mono's gate closes (child_held ⇒ mono_open ⇒ not from dead). In
//      this rig the precondition cannot hold; the derive is there per the ruling
//      (uniform + protective if the drive model ever changes).
//   holds / midClosed / the poly mid-hold path — MODE B NULLIFIES the internal
//      note length (executeModeB :2389-2405 clears holdRemain/gatePulseRemain
//      after the captures, before executeStep — the audible duration is Gate 1's
//      width, MODE_B_SPEC §3). The mono MidNote guard therefore never fires at a
//      stepped edge, and the poly mid-hold branch (the else of monoConnectLanding)
//      is correspondingly unreachable in this rig. The MidNote arm is UNCHANGED
//      by this fix (the doc's case 7) and stays pinned by the mode-A suites
//      (test_var_leg_rest, test_per_voice_articulation, test_modes_*); the
//      battery is the regression gate for it.
//
// Compile: see test/run_all.sh ("test_poly_tie_emergent").
#include <cstdio>
#include <cstdint>
#include <cmath>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "PhiloxRng.hpp"
#include "star_seam.hpp"

using redDot::PhiloxRng;
using dotModular::VoiceResolver;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static PatternInput makeInput(float variationAmount) {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount = variationAmount;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

static const int kLanes[7] = {
    SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST,   SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION,
    SequencerEngine::PL_LEGATO
};

// test_step_advance's seedEngine (lean local copy — the proven rig)
static void seedEngine(SequencerEngine& eng, PhiloxRng& rng) {
    for (int i = 0; i < 16; ++i) {
        eng.pe.melodyRandom[i]    = rng.nextFloat();
        eng.pe.octaveRandom[i]    = rng.nextFloat();
        eng.pe.qmixRandom[i]      = rng.nextFloat();
        eng.pe.rhythmRandom[i]    = rng.nextFloat();
        eng.pe.accentRandom[i]    = rng.nextFloat();
        eng.pe.variationRandom[i] = rng.nextFloat();
        eng.pe.legatoRandom[i]    = rng.nextFloat();
    }
    for (int v = 2; v <= 16; ++v) {
        const auto vs = VoiceResolver::slot(v);
        for (int lane = 0; lane < 7; ++lane)
            for (int i = 0; i < 16; ++i)
                eng.pe.polyRandom(vs, dotModular::LaneId{kLanes[lane]})[i] = rng.nextFloat();
        eng.voices[vs].restProb      = 0.10f + 0.55f * rng.nextFloat();
        eng.voices[vs].accentProb    = 0.60f * rng.nextFloat();
        eng.voices[vs].legatoProb    = 0.85f * rng.nextFloat();
        eng.voices[vs].variationProb = 1.0f;   // the identity point (no wander)
        eng.voices[vs].qmixLevel     = rng.nextFloat();
    }
    const auto m1 = VoiceResolver::slot(1);
    eng.voices[m1].restProb = 0.02f;           // mono almost never rests
    eng.voices[m1].legatoProb = 0.95f;         // mono slurs heavily -> landings
    eng.voices[m1].variationProb = 1.0f;
    eng.voices[m1].qmixLevel = 1.0f;           // mono (V1) all-quantised
}

struct GateStream {
    bool high = false;
    void step(PhiloxRng& rng, float pRise, float pFall, bool& rose) {
        rose = false;
        if (high) { if (rng.nextFloat() < pFall) high = false; }
        else      { if (rng.nextFloat() < pRise) { high = true; rose = true; } }
    }
};

static void forestDrive(SequencerEngine& eng, bool gateOpen, const bool* latch) {
    for (int k = 0; k < eng.rdm.topoCount; ++k) {
        const int i = eng.rdm.topoOrder[k];
        if (i == 0) {
            if (eng.rdm.parentCount[0] == 0) continue;
            bool monoOpen = gateOpen;
            for (int p = 0; p < eng.rdm.parentCount[0]; ++p) {
                const int u = eng.rdm.parentSlot[0][p];
                if (u > eng.numPolyVoices) continue;
                monoOpen = monoOpen && eng.voices[VoiceResolver::slot(u + 1)].gs.gateHeld;
            }
            eng.gs.gateHeld = monoOpen;
            if (!monoOpen) {
                eng.gs.holdRemain = 0.f;       eng.gs.gatePulseRemain = -1;
                eng.gsStep.holdRemain = 0.f;   eng.gsStep.gatePulseRemain = -1;
            }
            continue;
        }
        if (i > eng.numPolyVoices || eng.rdm.parentCount[i] == 0) continue;
        bool vOpen = latch[i + 1];
        for (int p = 0; p < eng.rdm.parentCount[i]; ++p) {
            const int u = eng.rdm.parentSlot[i][p];
            vOpen = vOpen && (u == 0 ? gateOpen
                                     : eng.voices[VoiceResolver::slot(u + 1)].gs.gateHeld);
        }
        const auto vs = VoiceResolver::slot(i + 1);
        eng.voices[vs].gs.gateHeld = vOpen;
        if (!vOpen) { eng.voices[vs].gs.holdRemain = 0.f; eng.voices[vs].gs.gatePulseRemain = -1; }
    }
}

// ── The CV schedule: the degree-1 voltage at a per-window octave ────────────────
// voiceNum 1 = mono (quantiserCV[0]); voiceNum v (2..16) = quantiserCV[v-1].
// Windows [0,w1) [w1,w2) [w2,STEPS): monoOct[] / voiceOct[v][] pick the octave.
static const int STEPS = 96;
struct Sched {
    int w1 = 40, w2 = 80;
    int monoOct[3] = {1, 2, 1};
    int voiceOct[17][3] = {};   // [voiceNum][window]
    int win(int step) const { return (step < w1) ? 0 : (step < w2) ? 1 : 2; }
    int oct(int voiceNum, int step) const {
        return (voiceNum == 1) ? monoOct[win(step)] : voiceOct[voiceNum][win(step)];
    }
};

// The degree-1 within-octave voltage, EXACTLY as the engine's 12-TET table
// composes it (o + cents[1]/1200), so the schedule snap is bit-comparable.
static float degVolts(bool retuned) { return (retuned ? 100.01f : 100.f) / 1200.f; }

struct TieCounters {
    int monoTie = 0, monoSlide = 0, monoNew = 0, holds = 0;
    int s1Tie = 0, s1Slide = 0, s3 = 0;
    int s2Tie = 0, s2Slide = 0;    // STRUCTURAL ZERO (see the header)
    int onsetArm = 0;              // the precondition count — STRUCTURAL ZERO in this rig
    int midClosedTotal = 0, midClosedStays = 0;   // MODE-B structural zero (the header)
};

static void printCounters(const char* tag, const TieCounters& c) {
    std::printf("  [%s] monoTie=%d monoSlide=%d monoNew=%d holds=%d | s1Tie=%d s1Slide=%d s3=%d | "
                "s2Tie=%d s2Slide=%d onsetArm=%d | midClosed=%d/%d\n",
                tag, c.monoTie, c.monoSlide, c.monoNew, c.holds,
                c.s1Tie, c.s1Slide, c.s3, c.s2Tie, c.s2Slide, c.onsetArm,
                c.midClosedStays, c.midClosedTotal);
}

// The march. legMax: mono legatoProb = 1.0 (every played step is a forced
// slideMax — LegatoMax, the join decision, UNTOUCHED by the change). retuneStep:
// the M3 microtuning retune (degree 1 +0.01 cents) applied between steps.
static TieCounters marchTie(const char* tag, uint32_t seed, const Sched& sch,
                            int nVoices, bool legMax, bool tieAcross, int retuneStep) {
    PhiloxRng rng; rng.seed64((uint64_t)seed * 747796405u + 2891336453u + 13u);
    SequencerEngine eng; eng.reset();
    injectLegacyStarGraph(eng);
    eng.numPolyVoices = nVoices;
    seedEngine(eng, rng);
    eng.pe.tuning.resetToEqual12TET();
    eng.quantiserPitchSource = true;
    eng.tieAcrossRests = tieAcross;
    const auto m1 = VoiceResolver::slot(1);
    eng.voices[m1].legatoProb = legMax ? 1.0f : 0.95f;
    for (int v = 2; v <= 16; ++v) {
        auto& pv = eng.voices[VoiceResolver::slot(v)];
        pv.qmixLevel = 1e9f;        // always the quantised CV (all-quantised)
        pv.qmixOctLive = false;     // the octave threshold == the degree threshold
        pv.restProb = 0.02f;
        pv.legatoProb = 0.90f;
    }
    // THE SCALE INGESTION (the rig-trap fix, see the header): without it the
    // cold quantize path leaves the memo poisoned with reset's 0.0.
    {
        float w[12];
        for (int i = 0; i < 12; ++i) w[i] = 1.f;
        eng.rebuildScaleCache(w, 12);
    }
    eng.voices[m1].qmixLevel = 1e9f;   // all-quantised, robustly (a warped q-mix draw can reach >= 1.0)
    // NOTE: Mode B nullifies the internal note length (the header), so the noteVal
    // base is inert here — kept for rig parity with the quantiser suite.
    const float noteVal = (float)legacyNoteValueIndex(1);   // the 1/2 end: the note variation stays grid-aligned, so slurForward commits and joins happen (at (4) the triplet draws break noteCanLeadLegato)
    const PatternInput in = makeInput(0.45f);
    GateStream main; bool prevMainHigh = false; bool prevStepGateOpen = false;
    bool latch[18] = {};
    TieCounters c;
    char msg[256];

    for (int step = 0; step < STEPS; ++step) {
        // the M3 retune (before the cascade): degree 1 +0.01 cents = +8.3e-6 V
        if (step == retuneStep) {
            eng.pe.tuning.cents[1] = 100.01f;
            eng.pe.tuning.recomputeDefaultFlag();
        }
        const bool retuned = (step >= retuneStep) && (retuneStep >= 0);
        // the CV writes: the schedule snap; M3 nudges +5e-4 post-retune (dodges
        // the quantize memo so the draw recomputes against the NEW table)
        const float nudge = retuned ? 5e-4f : 0.f;
        eng.quantiserCV[0] = (float)sch.oct(1, step) + degVolts(retuned) + nudge;
        for (int v = 2; v <= 16; ++v)
            eng.quantiserCV[v - 1] = (float)sch.oct(v, step) + degVolts(retuned) + nudge;

        // ── the pre-edge snapshots (the held side of every join) ──
        const float  monoPrePitch = eng.gs.currentPitchV;
        const int    monoPreSem   = eng.gs.lastSemitone;
        float vPrePitch[18]; int vPreSem[18]; bool vPreHeld[18];
        for (int v = 2; v <= 16; ++v) {
            const auto& pv = eng.voices[VoiceResolver::slot(v)];
            vPrePitch[v] = pv.gs.currentPitchV;
            vPreSem[v]   = pv.gs.lastSemitone;
            vPreHeld[v]  = pv.gs.gateHeld;
        }

        // ── the edge (the proven loop; DENSE stream: a rise every ~2.4 steps) ──
        bool rose; main.step(rng, 0.85f, 0.85f, rose);
        StepResult r = eng.executeModeB(rose, main.high, noteVal, in);
        if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
        for (int i = 1; i <= eng.numPolyVoices; ++i)
            latch[i + 1] = eng.voices[VoiceResolver::slot(i + 1)].gs.gateHeld;
        const bool isRest    = (r.decision == MonoDecision::Rest);
        const bool gapBridge = eng.tieAcrossRests ? eng.gs.slurForward : prevMainHigh;
        const bool gateOpen  = !isRest && (main.high || gapBridge);
        eng.gs.gateHeld = gateOpen; eng.gsStep.gateHeld = !isRest && main.high;
        if (!gateOpen) {
            eng.gs.holdRemain = 0.f; eng.gs.gatePulseRemain = -1;
            eng.gsStep.holdRemain = 0.f; eng.gsStep.gatePulseRemain = -1;
        }
        prevMainHigh = main.high;
        forestDrive(eng, gateOpen, latch);

        // ── MONO asserts (the join branch; LegatoMax = the forced slide, out of scope) ──
        const float schedSnapM = (float)sch.oct(1, step) + degVolts(retuned);
        if (r.stepped && r.decision == MonoDecision::MidNote) ++c.holds;   // MODE-B structural zero (the header)
        if (r.decision == MonoDecision::NewNote) ++c.monoNew;
        if (r.decision == MonoDecision::Tie || r.decision == MonoDecision::Legato) {
            // THE RULE from the schedule: same degree (always 1 here — assert the
            // held degree names it too) AND same octave (schedule vs the held).
            const bool sameDeg  = (monoPreSem == 1);
            const int  heldOct  = (int)std::floor(monoPrePitch);
            const bool expectedTie = sameDeg && (heldOct == sch.oct(1, step));
            if (expectedTie) {
                ++c.monoTie;
                std::snprintf(msg, sizeof msg, "%s step %d: mono expected TIE (degree %d, octave %d held, %d drawn) got %d",
                              tag, step, monoPreSem, heldOct, sch.oct(1, step), (int)r.decision);
                CHECK(r.decision == MonoDecision::Tie, msg);
                std::snprintf(msg, sizeof msg, "%s step %d: mono tie must keep the held pitch (%f -> %f)",
                              tag, step, (double)monoPrePitch, (double)eng.gs.currentPitchV);
                CHECK(std::fabs(eng.gs.currentPitchV - monoPrePitch) < 1e-6f, msg);
                // the label the display/jack path reads + the green quant flash
                CHECK(eng.gs.lastNoteType == GateState::NoteType::Tie,
                      "mono tie: lastNoteType == Tie (the Lantern/jack label)");
                if (eng.gs.lastSemitone >= 0) {
                    std::snprintf(msg, sizeof msg, "%s step %d: mono tie must mark the QUANT flash timer (isQuant)",
                                  tag, step);
                    CHECK(eng.gs.semiQuantPlayRemain[eng.gs.lastSemitone] > 0.f, msg);
                }
            } else {
                ++c.monoSlide;
                std::snprintf(msg, sizeof msg, "%s step %d: mono expected LEGATO (slide, degree %d, octave %d held -> %d drawn) got %d",
                              tag, step, monoPreSem, heldOct, sch.oct(1, step), (int)r.decision);
                CHECK(r.decision == MonoDecision::Legato, msg);
                std::snprintf(msg, sizeof msg, "%s step %d: mono slide must land on the new draw %f (got %f — the octave must NOT be discarded)",
                              tag, step, (double)schedSnapM, (double)eng.gs.currentPitchV);
                CHECK(std::fabs(eng.gs.currentPitchV - schedSnapM) < 1e-4f, msg);
                CHECK(eng.gs.lastNoteType == GateState::NoteType::Legato,
                      "mono slide: lastNoteType == Legato (the Lantern/jack label)");
            }
        }

        // ── POLY asserts ──
        const bool landing = (r.decision == MonoDecision::Legato)
                          || (r.decision == MonoDecision::LegatoMax)
                          || (r.decision == MonoDecision::Tie);
        const bool monoFromDead = landing && !prevStepGateOpen;   // the onset-path precondition
        for (int v = 2; v <= nVoices; ++v) {
            const auto vs = VoiceResolver::slot(v);
            const auto& pv = eng.voices[vs];
            const MonoDecision verdict = eng.polyVerdict_[vs.v];
            const float schedSnapV = (float)sch.oct(v, step) + degVolts(retuned);

            // the MidNote closed-window invariant (case 7) — MODE-B structural zero
            // in this rig (the header): the arm cannot be reached here; counted,
            // reported, and pinned by the mode-A suites instead.
            if (r.stepped && r.decision == MonoDecision::MidNote && !vPreHeld[v]) {
                ++c.midClosedTotal;
                if (!pv.gs.gateHeld) ++c.midClosedStays;
                else {
                    std::snprintf(msg, sizeof msg, "%s step %d v%d: MidNote arm re-opened a closed gate window",
                                  tag, step, v);
                    CHECK(false, msg);
                }
            }

            // the S2 territory (mono mid-hold, this voice sounding): the merged
            // arm's would-be outputs — STRUCTURALLY ZERO (see the header)
            if (r.stepped && r.decision == MonoDecision::MidNote && vPreHeld[v]) {
                if (verdict == MonoDecision::Tie) ++c.s2Tie;
                if (verdict == MonoDecision::Legato) ++c.s2Slide;
            }

            if (!landing) continue;
            // the onset-arm precondition (mono legatoed FROM DEAD while this
            // voice still sounds) — STRUCTURALLY ZERO in this rig (the header)
            if (monoFromDead && vPreHeld[v]) ++c.onsetArm;

            if (verdict == MonoDecision::NewNote) {
                ++c.s3;   // the re-articulate arm (a landing, mono held)
                if (!monoFromDead) {
                    std::snprintf(msg, sizeof msg, "%s step %d v%d: re-articulate must draw its own CV %f (got %f)",
                                  tag, step, v, (double)schedSnapV, (double)pv.gs.currentPitchV);
                    CHECK(std::fabs(pv.gs.currentPitchV - schedSnapV) < 1e-4f, msg);
                }
                continue;
            }
            if (verdict != MonoDecision::Tie && verdict != MonoDecision::Legato) continue;

            // THE RULE from the voice's OWN schedule (never mono's verdict)
            const bool sameDeg = (vPreSem[v] == 1);
            const int  heldOct = (int)std::floor(vPrePitch[v]);
            const bool expectedTie = sameDeg && (heldOct == sch.oct(v, step));
            if (expectedTie) {
                ++c.s1Tie;
                std::snprintf(msg, sizeof msg, "%s step %d v%d: expected the voice's OWN TIE (oct %d held, %d drawn; mono=%d) got %d",
                              tag, step, v, heldOct, sch.oct(v, step), (int)r.decision, (int)verdict);
                CHECK(verdict == MonoDecision::Tie, msg);
                std::snprintf(msg, sizeof msg, "%s step %d v%d: voice tie must keep its held pitch (%f -> %f)",
                              tag, step, v, (double)vPrePitch[v], (double)pv.gs.currentPitchV);
                CHECK(std::fabs(pv.gs.currentPitchV - vPrePitch[v]) < 1e-6f, msg);
                if (pv.gs.lastSemitone >= 0) {
                    std::snprintf(msg, sizeof msg, "%s step %d v%d: voice tie must mark the QUANT flash timer (isQuant on every arm)",
                                  tag, step, v);
                    CHECK(pv.gs.semiQuantPlayRemain[pv.gs.lastSemitone] > 0.f, msg);
                }
            } else {
                ++c.s1Slide;
                std::snprintf(msg, sizeof msg, "%s step %d v%d: expected the voice's OWN SLIDE to %f (oct %d held, %d drawn; mono=%d) got %d pitch %f",
                              tag, step, v, (double)schedSnapV, heldOct, sch.oct(v, step),
                              (int)r.decision, (int)verdict, (double)pv.gs.currentPitchV);
                CHECK(verdict == MonoDecision::Legato, msg);
                CHECK(std::fabs(pv.gs.currentPitchV - schedSnapV) < 1e-4f, msg);
            }
        }
        prevStepGateOpen = gateOpen;
    }
    printCounters(tag, c);
    return c;
}

int main() {
    std::printf("test_poly_tie_emergent: tie is emergent — same degree AND same octave at every join (the star-graph march)\n");
    char msg[256];

    // ═══ M1: the MONO cases — the octave bug + the same-pitch tie ═══
    {
        Sched sch; sch.w1 = 40; sch.w2 = 80;
        sch.monoOct[0] = 1; sch.monoOct[1] = 2; sch.monoOct[2] = 1;
        for (int v = 2; v <= 16; ++v) { sch.voiceOct[v][0] = 1; sch.voiceOct[v][1] = 1; sch.voiceOct[v][2] = 1; }
        const TieCounters c = marchTie("M1", 11u, sch, /*nVoices=*/4, /*legMax=*/false, /*tieAcross=*/true, /*retune=*/-1);
        std::snprintf(msg, sizeof msg, "M1 coverage: monoTie=%d monoSlide=%d (the mono join arm must be reached)",
                      c.monoTie, c.monoSlide);
        CHECK(c.monoTie > 0 && c.monoSlide > 0, msg);
    }

    // ═══ M2: the POLY matrix — independence both directions ═══
    {
        Sched sch; sch.w1 = 32; sch.w2 = 64;
        // mono: 1,1,2 — ties through W1/W2, slides into W3.
        sch.monoOct[0] = 1; sch.monoOct[1] = 1; sch.monoOct[2] = 2;
        for (int v = 2; v <= 16; ++v) {
            const int base = 1 + (v % 2);          // W1: evens oct1, odds oct2
            sch.voiceOct[v][0] = base;
            sch.voiceOct[v][1] = (v % 2 == 0) ? 3 - base : base;  // W2: evens FLIP (mono ties!), odds hold
            sch.voiceOct[v][2] = (v % 2 == 0) ? 3 - base : 3 - base; // W3: odds FLIP (mono slides), evens hold
        }
        const TieCounters c = marchTie("M2", 23u, sch, /*nVoices=*/15, /*legMax=*/false, /*tieAcross=*/true, /*retune=*/-1);
        std::snprintf(msg, sizeof msg, "M2 coverage: s1Tie=%d s1Slide=%d s3=%d monoTie=%d monoSlide=%d",
                      c.s1Tie, c.s1Slide, c.s3, c.monoTie, c.monoSlide);
        CHECK(c.s1Tie > 0 && c.s1Slide > 0 && c.s3 > 0 && c.monoTie > 0 && c.monoSlide > 0, msg);
    }

    // ═══ M3: the EPSILON case — a microtuning retune keeps the tie ═══
    // NOT red on the old engines (the degree-only compare also ties it): this is
    // the mutation-(d) catcher — epsilon 0 turns this tie into a slide.
    {
        Sched sch; sch.w1 = 40; sch.w2 = 80;
        sch.monoOct[0] = 1; sch.monoOct[1] = 1; sch.monoOct[2] = 1;   // constant octave
        for (int v = 2; v <= 16; ++v) { sch.voiceOct[v][0] = 1; sch.voiceOct[v][1] = 1; sch.voiceOct[v][2] = 1; }
        const TieCounters c = marchTie("M3", 37u, sch, /*nVoices=*/4, /*legMax=*/false, /*tieAcross=*/true, /*retune=*/40);
        std::snprintf(msg, sizeof msg, "M3 coverage: monoTie=%d (the retuned degree-1 joins must all tie, |dV|=%.1e V)",
                      c.monoTie, (double)(0.01f / 1200.f));
        CHECK(c.monoTie > 0, msg);
    }

    // ═══ M4: LEGATOMAX — the forced slide stays; the S1 same-pitch relabel ═══
    {
        Sched sch; sch.w1 = 32; sch.w2 = 64;
        sch.monoOct[0] = 1; sch.monoOct[1] = 1; sch.monoOct[2] = 1;   // constant: the same-pitch joins
        for (int v = 2; v <= 16; ++v) { sch.voiceOct[v][0] = 1; sch.voiceOct[v][1] = 1; sch.voiceOct[v][2] = 1; }
        const TieCounters c = marchTie("M4", 53u, sch, /*nVoices=*/15, /*legMax=*/true, /*tieAcross=*/false, /*retune=*/-1);
        std::snprintf(msg, sizeof msg, "M4 coverage: s1Tie=%d s3=%d (LegatoMax landings must reach S1)",
                      c.s1Tie, c.s3);
        CHECK(c.s1Tie > 0 && c.s3 > 0, msg);
        // the structural zeros — printed above by printCounters; NOT asserted (the ruling)
    }

    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
