// ─────────────────────────────────────────────────────────────────────────────
// LanternRecordCore — Lantern's recording state, RACK-FREE (the
// CantonmentModuleCore precedent: the module-state core is testable headless,
// the module shell wraps it). Everything here is the DISPLAY OBSERVER's own
// state — it reads engine state and writes NOTHING back.
//
// STEPS_32 STAGE 3 (the DECIDED Lantern blocks in docs/design/STEPS_32_PLAN.md):
//   • 16 COLUMNS AT EVERY PATTERN LENGTH. No 32-column mode (legibility).
//   • FOLLOWS THE DISC: the displayed bar is the ENGINE's absolute bar — the
//     same index the inner-disc bar indicator shows (BAR INDICATOR invariant 1:
//     display reads engine position, never recomputes it). bar = stepIndex / 16.
//   • BOTH BARS RECORDED: the write path is bar-aware — bar = step / 16,
//     column = step % 16 — so steps 16..31 land in the bar-2 buffer instead of
//     being dropped by the old bar-blind `step >= 16` guard. Two 16-column
//     buffers (the DECIDED option), so switching the view never loses the other
//     bar's history.
//   • LATER (not here): a manual bar switch and/or a freeze-display option —
//     both view-only, no recording-path change.
//
// THE RECORDING LOGIC IS VERBATIM from src/Lantern.cpp (the module keeps the
// widget + the rack-facing IT-source branch); only the storage gained the bar
// axis and the write routing gained the bar split. At a ≤16 pattern every edge
// resolves writeBar == 0, so the bar-1 (first) buffer holds the FULL history —
// the ≤16 output is unchanged by construction. The record path reads
// SequencerEngine/GateState/MonoDecision — all rack-free engine types the
// headless battery already compiles.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <algorithm>
#include <cmath>

#include "../dsp/engines/RhythmDecisionCore.hpp"// MonoDecision (rack-free)
#include "../dsp/engines/SequencerEngine.hpp"   // the engine state the observer reads (rack-free)
#include "../dsp/gates/GateState.hpp"
#include "../dsp/engines/PatternEngine.hpp"     // dotModular::kMaskRing / kPatternLengthCap (STEPS_32)
#include "../dsp/VoiceResolver.hpp"             // VoiceResolver::slot — the poly row pairing

namespace lantern {
    // Note-name helper (self-contained; no engine change) ─────────────────────
    // 1V/oct → "C4" style label. Rack convention: 0 V = C4.
    inline std::string noteName(float volts) {
        static const char* N[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        int semis = (int)std::lround(volts * 12.0f);     // semitones from C4
        int idx   = ((semis % 12) + 12) % 12;
        int oct   = 4 + (int)std::floor((semis + (semis < 0 ? -0 : 0)) / 12.0);
        // floor division for octave (handles negatives)
        oct = 4 + (int)std::floor(semis / 12.0);
        return std::string(N[idx]) + std::to_string(oct);
    }

    // Note-type for the colour axis, mapped from the engine's MonoDecision.
    // Accent is NOT here — it is an orthogonal overlay flag on the Cell.
    enum class NoteType : uint8_t { Inactive, Single, Tie, Legato };
}

struct LanternRecordCore {
    // ── The view's constants (DECIDED) ────────────────────────────────────────
    // The bar is ALWAYS 16 steps (BAR INDICATOR invariant 3: pattern length
    // changes only how many bars the phrase has, never what a bar means) and
    // the view stays 16 columns wide. N_BARS covers the ring: two 16-column
    // buffers at the 32 ring. When the 64-step display is decided, N_BARS
    // widens with the ring — the static_assert makes that a conscious change.
    static constexpr int N_VOICES  = 16;
    static constexpr int BAR_STEPS = 16;
    static constexpr int N_BARS    = 2;
    static_assert(N_BARS * BAR_STEPS >= dotModular::kPatternLengthCap,
                  "the two-bar display buffer must cover the pattern ring (STEPS_32)");
    static_assert(dotModular::kPatternLengthCap % BAR_STEPS == 0 || N_BARS * BAR_STEPS > dotModular::kPatternLengthCap,
                  "a partial trailing bar is fine; an uncovered ring cell is not");

    // ── Per-voice display ring buffer — Lantern's OWN accumulated history (the
    // engine only keeps lastStepResult, so we record what we observe each step).
    // [bar][voice][step]: bar 0 = pattern steps 0..15, bar 1 = steps 16..31
    // (the DECIDED both-bars-recorded layout). The displayed bar follows the
    // disc; the OTHER bar keeps recording underneath.
    struct Cell {
        lantern::NoteType type = lantern::NoteType::Inactive;
        float  pitchV   = 0.f;   // for the label
        float  lengthSteps = 0.f;// TRUE length in 1/16 steps from the gate pulse count
                                 // (gatePulseRemain / pulsesPer16th at this step edge) —
                                 // so triplets/1/32 and tied runs ending on them keep their
                                 // real sub-step end, not the nominal note-value length.
        bool   heldIn   = false; // continues from the previous bar
        bool   heldOut  = false; // continues past the bar's last column
        bool   accented = false;
        bool   slurMember = false;// this note is PART OF a slur (leads one, or continues one) —
                                 // the engine's gs.slurMember, the exact SLEG output mask. Drawn
                                 // as a thin underline so an underlined RUN of cells = one fused
                                 // GATE (lead + continuations), and its cell edges = the STEP
                                 // strikes inside it. Isolated notes: no underline.
        bool   leadsLegato = false;// this note INITIATES a legato lead: it committed slurForward
                                 // (will hold its gate into the next note) AND is not itself a
                                 // received Tie/Legato (the chain interior is already teal/violet).
                                 // Drawn as an outline/edge marker over its normal fill colour.
        bool   isMidTail = false;// MidNote gate-tail (a longer note still ringing across this
                                 // step edge) — NOT a new event. The onset cell's true
                                 // fractional width already covers it, so the render skips
                                 // drawing a bar here (else it fills the sub-step gap).
        int    playDir  = +1;    // play direction when this cell was recorded (+1 fwd / -1 rev).
                                 // A short (<1 step) note's bar is anchored toward the play
                                 // direction so its fill reaches toward the note it connects
                                 // FROM: forward → left-anchored (extends right); reverse →
                                 // right-anchored (extends left). Left-anchoring in reverse
                                 // leaves the note's empty right portion as a visual gap on the
                                 // play-direction side, making short legato notes look isolated.
    };
    std::array<std::array<std::array<Cell, BAR_STEPS>, N_VOICES>, N_BARS> cells;

    // Bookkeeping. lastObservedStep / lastWriteStepObs are ABSOLUTE pattern steps
    // (0..kMaskRing-1) — the lap is the PHRASE (the ring), not the bar: the bar
    // boundary 15→16 is mid-lap travel (a bar crossing, the view follows the
    // disc), the ring wrap 31→0 is the lap the wrap-arrival tail cares about.
    int lastObservedStep = -1;    // the absolute playhead at the last step edge (the sample key)
    int lastWriteStepObs = -1;    // previous ABSOLUTE write step, for lap-wrap (arrival) detection
    int displayBar = 0;           // FOLLOWS THE DISC: the engine's absolute bar (stepIndex / 16)
    int displayCol = -1;          // the playhead column within the displayed bar (-1 = none)

    // ── One step edge's frozen context: the engine reads the record path uses,
    // plus the resolved write slot. Produced by beginSample; consumed by
    // recordGlobalVoice / recordMonsoonSource / the module's IT-source branch
    // (all three must route through the SAME edge — one spelling of the slot).
    struct StepEdge {
        bool valid = false;       // false: not a step edge, or the playhead is outside the ring
        int  writeBar = 0;        // bar = absolute step / BAR_STEPS (the DECIDED routing)
        int  writeCol = 0;        // column = absolute step % BAR_STEPS
        bool lapArrival = false;  // the write crossed the PHRASE (ring) boundary against travel
        MonoDecision dec{};       // mono's decision (the Lantern pairing reads — see the module)
        bool accentedMono = false;
        float lenSteps = 0.f;
        bool monoSlur = false;
    };

    // Read-only sampler entry: records the observed per-step state into the
    // display buffers on each step edge. Per-step, as-it-happens, NO look-back — a
    // note reads as Single when it fires and only changes to Tie/Legato at the step
    // where the engine's decision actually becomes a continuation (the 2nd note on).
    // This mirrors what Monsoon really does (probabilistic, not pre-planned), so the
    // colour change AT the join is the truthful tie/legato signal. Trusts MonoDecision.
    // Reads engine state only; writes nothing back.
    //
    // THE FOLLOW-DISC BAR (DECIDED): displayBar/displayCol are updated on EVERY
    // step edge from the engine's absolute playhead — BEFORE the record guard —
    // so the view follows the disc even on an edge the record path skips, and the
    // flip lands on the SAME tick the playhead lands in the new bar (BAR INDICATOR
    // invariant 2's flip timing, from the display side).
    StepEdge beginSample(const SequencerEngine& eng) {
        StepEdge e;
        const int step = eng.stepIndex;
        if (step == lastObservedStep) return e;   // only sample on step edges
        lastObservedStep = step;
        const bool inRing = (step >= 0 && step < dotModular::kMaskRing);
        // THE BAR IS ALWAYS 16 STEPS: the absolute bar is the playhead's sixteenth
        // pattern bar (stepIndex / 16) — the same index the disc shows.
        displayBar = inRing ? (step / BAR_STEPS) : displayBar;
        displayCol = inRing ? (step % BAR_STEPS) : -1;
        if (!inRing) return e;   // outside the ring (pre-first-step): nothing to record

        e.dec          = eng.lastStepResult.decision;
        e.accentedMono = eng.lastStepResult.accented;
        e.lenSteps     = gs_noteSteps(eng.lastStepResult.nvIdx);
        e.monoSlur     = eng.gs.slurForward;   // the mono gate's forward-hold commitment;
                                               // poly voices follow mono's gate, so inherit it

        // Pair the decision with the column it was actually COMPUTED for. Lantern samples when
        // eng.stepIndex changes, but stepIndex is advanced (by advancePlayhead) BEFORE the
        // engine computes that step's decision into lastStepResult — so at the sample instant
        // stepIndex can already point at the new step while lastStepResult still holds the
        // PREVIOUS step's decision. Writing dec into cells[stepIndex] then paints the wrong
        // column (the previous step's decision lands in the new step's cell → e.g. a real note's
        // cell renders empty/rest). This is direction-sensitive and is the reverse isolated-teal.
        // Fix: write into the decision's OWN step (lastStepResult.forStep), so a decision always
        // lands in its own column regardless of sampling phase. ABSOLUTE form: the bar rides the
        // same split (bar = step / 16, column = step % 16 — the DECIDED bar-aware write path).
        const int absWrite = (eng.lastStepResult.forStep >= 0 && eng.lastStepResult.forStep < dotModular::kMaskRing)
                             ? eng.lastStepResult.forStep : step;
        e.writeBar = absWrite / BAR_STEPS;
        e.writeCol = absWrite % BAR_STEPS;

        // LAP ARRIVAL: the playhead crossed the lap boundary since the last sample — the write
        // step moved AGAINST the travel direction (forward: new < old; reverse: new > old). A
        // note that held its gate OUT of the previous lap is still sounding here, but its onset
        // bar lives on the PREVIOUS lap's cells (clipped at the far edge) and cannot cover this
        // lap's tail — so without special handling the tail renders as nothing (isMidTail skip)
        // and a legato join landing on it looks like teal-after-a-rest. recordCell turns the
        // FIRST tail cell of the lap into a drawable continuation bar instead. (A jump that
        // moves against travel also flags this; its covering onset is likewise elsewhere, so
        // drawing the tail is the truthful render there too.)
        // ABSOLUTE form: the BAR crossing (15→16 fwd / 16→15 rev) is NOT a lap — both bars'
        // buffers are adjacent halves of one phrase, and the crossing note's onset is fully
        // visible in the bar-1 view (the DECIDED "held-over carets mark notes that continue
        // across the bar boundary"); only the RING wrap (31→0 fwd / 0→31 rev) is a lap.
        e.lapArrival = (lastWriteStepObs >= 0) && (eng.lastPlayDir < 0
                       ? (absWrite > lastWriteStepObs)
                       : (absWrite < lastWriteStepObs));
        lastWriteStepObs = absWrite;
        e.valid = true;
        return e;
    }

    // Record a GLOBAL voice's state into a display ROW. Global voice 0 = mono/V1 (eng.gs);
    // global voice g>=1 = poly voice V(g+1), i.e. VoiceSlot{g} = slot(g+1) (post-COLLAPSE:
    // slot 0 is V1's dormant slot, poly voices V2..V16 are slots 1..15). Shared by both
    // source modes; only the row<->voice pairing differs. Writes cells[e.writeBar][row][e.writeCol].
    void recordGlobalVoice(const SequencerEngine& eng, const StepEdge& e, int row, int g) {
        if (row < 0 || row >= N_VOICES) return;
        if (g == 0) {
            recordCell(row, e, eng.gs, e.dec, e.accentedMono, e.lenSteps, e.monoSlur, e.monoSlur,
                       eng.lastPlayDir, e.lapArrival);
        } else {
            const int pv_i = g - 1;
            if (pv_i < 0 || pv_i >= eng.numPolyVoices || pv_i >= 15) {
                cells[e.writeBar][row][e.writeCol].type = lantern::NoteType::Inactive; return;
            }
            // OFF-BY-ONE FIX (audit item 2, confirmed in Rack — display row 2 missing,
            // rows one-voice-shifted). Global poly voice g (g>=1) IS poly voice V(g+1):
            // g=1 → V2 = VoiceSlot{1} = slot(2) = slot(pv_i+2). Was slot(pv_i+1) =
            // slot(g) = VoiceSlot{0} for g=1 = V1's DORMANT per-voice slot (post-COLLAPSE
            // mono runs on eng.gs), so row 2 read a dead slot and later rows showed the
            // previous voice.
            const PolyVoice& pv = eng.voices[dotModular::VoiceResolver::slot(pv_i + 2)];
            const bool voiceSlur = pv.gs.slurForward;   // SANDS CONSOLIDATION Step 5: per-voice slur (flag removed)
            recordCell(row, e, pv.gs, e.dec, pv.accented, e.lenSteps, voiceSlur, e.monoSlur,
                       eng.lastPlayDir, e.lapArrival);
        }
    }

    // The MONSOON-SOURCE (identity) march: row 0 = mono/V1, rows 1..numPolyVoices = poly voices.
    void recordMonsoonSource(const SequencerEngine& eng, const StepEdge& e) {
        recordGlobalVoice(eng, e, 0, 0);
        for (int v = 0; v < eng.numPolyVoices && v < N_VOICES - 1; ++v)
            recordGlobalVoice(eng, e, v + 1, v + 1);
        for (int v = eng.numPolyVoices; v < N_VOICES - 1; ++v)
            cells[e.writeBar][v + 1][e.writeCol].type = lantern::NoteType::Inactive;
    }

    // Map an observed voice state at a step into a display Cell. NoteType priority:
    // inactive (gate down & not held) → else Accent if accented → else the join type
    // (Tie/Legato) from the decision → else Single (a fresh/normal note).
    // monoLeading = mono's slurForward (whether mono is leading a slur). Used to gate the
    // poly lead marker so a per-voice lead never floats outside mono's chain.
    // (VERBATIM from Lantern.cpp's recordCell; only the storage axis gained the bar.)
    void recordCell(int voice, const StepEdge& e, const GateState& gs, MonoDecision dec,
                    bool accented, float lenSteps, bool slurFwd, bool monoLeading, int playDir,
                    bool lapArrival = false) {
        Cell& c = cells[e.writeBar][voice][e.writeCol];
        // REST is AUTHORITATIVE over the timing-sensitive gate read (MODE_B_SPEC.md
        // "REMAINING ISSUE"). In Mode B the gate STATE (gs.gateHeld/holdRemain) is corrected by
        // Monsoon's per-sample gate driver, but Rack's module processing order is not fixed: if
        // Lantern runs BEFORE Monsoon in a block it samples the PRE-correction gate (still high
        // from the note's triggerNote), and since stepIndex doesn't change again until the next
        // rise, the corrected rest state is never observed -> rests vanish from the display while
        // the scope shows them. eng.lastStepResult.decision is set ATOMICALLY inside executeStep
        // and persists until the next step, so it is race-free — trust it over gateHeld/holdRemain.
        //   Applies to ALL voices (mono + poly): on MonoDecision::Rest the engine forces
        //   v.gs.gateHeld=false + v.participating=false for EVERY poly voice too (executePolyVoice
        //   rest branch, ~:761-771) — poly CANNOT initiate a note when mono rests (it CAN rest
        //   while mono plays, but that is dec!=Rest and correctly falls through to the gate test).
        //   So the unguarded early-return is both correct and race-proof: it also catches a poly
        //   voice left with a stale gateHeld=true by the same processing-order race.
        // General + harmless outside Mode B: in Mode A/E dec==Rest iff the gate is truly down.
        if (dec == MonoDecision::Rest) { c.type = lantern::NoteType::Inactive; return; }
        // A cell sounds if THIS voice's own gate is up. recordCell is called for mono AND each
        // poly voice with the SAME mono 'dec', so 'dec' must NOT drive sounding — a poly voice
        // that independently rested has its own gate down and must render blank even though mono
        // fired. (An earlier decFired shortcut broke poly rests by rendering every voice whenever
        // mono fired.) The reverse short-note isolated-teal that decFired was meant to address is
        // fixed properly by direction-aware bar anchoring in draw(), so this stays gate-based.
        const bool sounding = gs.gateHeld || gs.holdRemain > 0.0001f;
        if (!sounding) { c.type = lantern::NoteType::Inactive; return; }
        // HELD-OVER TAIL: this voice did NOT fire a new onset this step, so its lastNoteType is
        // STALE from the previous note. Rendering it as a fresh Legato/Single cell produced a
        // stray "legato note outside the mono chain" — e.g. a teal poly cell when mono had moved
        // on to a fresh blue NewNote. Two detectable forms:
        //   (a) the voice's audible gate already closed (gatePulseRemain <= 0 — tickPulse ran it
        //       down) but a stale holdRemain > 0 lingers (the poly rest-branch re-opens gateHeld
        //       from holdRemain alone, without re-arming the pulse timer);
        //   (b) at a mono NewNote, a voice that PLAYED always calls triggerNote → lastNoteType =
        //       Single. So a sounding voice whose lastNoteType is NOT Single at a NewNote did NOT
        //       fire this step — it rested while its previous note's tail still rings (the gate
        //       may still be armed, so (a) alone misses it). Its Legato/Tie type is stale from the
        //       previous chain. Treat both as a continuation tail (isMidTail): keep the old base
        //       colour, draw no fresh type and no lead marker.
        const bool heldOverTail = (gs.gatePulseRemain <= 0) && (gs.holdRemain > 0.0001f);

        // WRAP-ARRIVAL TAIL: this is the first cell of a new lap and this voice is mid-tail —
        // the note it continues started LAST lap, so no onset bar this lap can cover it. Render
        // THIS cell as a continuation bar (remaining length from the gate countdown, the note's
        // own type colour, held-in caret, no onset tick) instead of skipping it. Later tail
        // cells of the same note stay isMidTail — this bar's fractional width covers them.
        const bool wrapTail = lapArrival
                           && (dec == MonoDecision::MidNote || heldOverTail)
                           && (gs.gatePulseRemain > 0);

        c.pitchV      = gs.currentPitchV;
        c.playDir     = (playDir < 0) ? -1 : +1;
        // TRUE fractional length from the gate's pulse countdown (the sole gate-close
        // mechanism). At a step edge just after (re)arming, gatePulseRemain = the pulses
        // this gate will stay open, so /pulsesPer16th gives the real length in 1/16 steps
        // — 1.333 for a 1/8T, 0.5 for a 1/32, and the summed remainder for a tie ending on
        // one. Falls back to nominal lenSteps if the pulse count isn't populated.
        {
            const int p16 = gs.pulsesPer16th > 0 ? gs.pulsesPer16th : 6;
            c.lengthSteps = (gs.gatePulseRemain > 0)
                            ? (float)gs.gatePulseRemain / (float)p16
                            : lenSteps;
        }
        c.accented    = accented;   // orthogonal overlay (render brightens/marks), NOT a type
        // SLEG membership: the voice's OWN slurMember (set at every articulation alongside
        // lastNoteType) — per-voice for the same reason type is. A held-over tail's value is
        // stale from the previous note, but such cells are isMidTail and the render skips
        // them, so no gating needed here.
        c.slurMember  = gs.slurMember;
        // heldIn = the gate carried over from the previous bar. This is about RHYTHM
        // continuity (the gate held across the boundary), which is identical for a tie and a
        // legato — the only difference between them is whether the note CV changes (Legato =
        // held gate + new pitch; Tie = held gate + same pitch). So the held-in caret must fire
        // for BOTH. (Previously only Tie/MidNote, so a cross-boundary legato showed teal with
        // no caret and looked isolated at step 0 — confirmed on Rack's scope as genuinely held
        // over the boundary.) MidNote is the tail of an already-shown note; also continues.
        c.heldIn      = (gs.lastNoteType == GateState::NoteType::Tie ||
                         gs.lastNoteType == GateState::NoteType::Legato ||
                         dec == MonoDecision::MidNote)
                      || wrapTail;   // arrival bar: the note carried over the lap boundary
        // heldOut = this note holds its gate high PAST the last step, by EITHER mechanism:
        //   (1) a LONG note whose length extends past this step (holdRemain > 1), or
        //   (2) a LEGATO LEAD that committed to hold its gate forward (slurFwd). With
        //       The voice rolls its OWN slurForward (per-voice articulation is always-on;
        //       flag removed in SANDS CONSOLIDATION Step 5).
        //       (Rule 2), so slurFwd is that per-voice value. Either way it is the correct
        //       forward-hold for THIS voice. A poly voice that RESTED already early-returned
        //       Inactive above, so this marks only sounding voices. Pairs with the step-0
        //       held-in caret next lap to show the wraparound: leaves right edge, arrives left.
        // (heldIn already only fires on a real connection — a rest at step 0 early-returns as
        //  Inactive before heldIn is set, and Tie/Legato/LegatoMax/MidNote are all genuine
        //  connections — so the two carets are symmetric and truthful.)
        c.heldOut     = (gs.holdRemain > 1.0001f) || slurFwd;
        c.isMidTail   = ((dec == MonoDecision::MidNote) || heldOverTail)
                      && !wrapTail;   // arrival bar draws; later tail cells stay covered by it

        // Note-TYPE is single/tie/legato only (accent is a separate overlay so an
        // accented tie still reads as a tie). Read the VOICE'S OWN articulation
        // (gs.lastNoteType), not mono's decision — so a poly voice that opted out and
        // re-struck inside a slur reads Single (blue) while a connecting voice reads
        // Legato/Tie. This is what makes per-voice legato visible; mono (row 0) reads
        // eng.gs.lastNoteType and is unchanged. (Set by triggerNote/slideNote/extendHold.)
        // A MidNote is the tail of an already-shown note — keep the old base colour for it
        // (isMidTail drives its rendering as a continuation), so tails are unchanged.
        if (dec == MonoDecision::MidNote && !wrapTail) {
            c.type = lantern::NoteType::Single;
        } else switch (gs.lastNoteType) {
            case GateState::NoteType::Tie:    c.type = lantern::NoteType::Tie;    break;
            case GateState::NoteType::Legato: c.type = lantern::NoteType::Legato; break;
            case GateState::NoteType::Single:
            default:                          c.type = lantern::NoteType::Single; break;
        }

        // Lead marker: this note INITIATES a legato — it committed slurFwd (its own under
        // Rule 2, mono's when following) and is itself a fresh start (Single), NOT a received
        // Tie/Legato (those are chain interior, already teal/violet). Gated on monoLeading so a
        // poly voice's per-voice slurForward (rolled at e.g. a mono Tie where mono is just holding,
        // monoSlur=0) does NOT draw a lead outline floating outside mono's chain — the "legato lead
        // outside the mono envelope" symptom. For mono (row 0) slurFwd == monoLeading, so this is
        // unchanged. A held-over tail is never a lead (stale slurFwd, continuation not a chain start).
        c.leadsLegato = slurFwd && monoLeading && (c.type == lantern::NoteType::Single)
                     && !heldOverTail && !wrapTail;   // the arrival is a continuation, its onset
                                                      // (last lap) already carries the outline
    }
};
