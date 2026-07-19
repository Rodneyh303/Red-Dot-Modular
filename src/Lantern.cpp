// ─────────────────────────────────────────────────────────────────────────────
// Lantern — note-output visualiser (read-only observer expander)
//
// "Lights up what's happening." A 16-lane bar view of the notes Monsoon is
// generating: one narrow lane per voice (row N = voice N), bars over the 16-step
// grid, colour = note TYPE, sub-step-resolution ends, gate-start ticks, and
// held-over carets for notes spanning the bar boundary.
//
// ── COLOUR CONVENTION ────────────────────────────────────────────────────────
//   BLUE   (0x6c8cd4) = Single  — a normal new note (NewNote onset)
//   VIOLET (0x8a6cd4) = Tie     — held / joined to the previous note (same pitch)
//   TEAL   (0x26a69a) = Legato  — slid to a new pitch without a retrigger
//   GREY   (0x3a4048) = inactive / voice not sounding
//   Accent is ORTHOGONAL (decided at the note's start, carried through a tie/
//   legato): an accented note is a BRIGHTER SHADE of its type colour (lerp 0.28
//   toward white) plus a thin brand-red (0xd4001a) top edge; non-accented is
//   slightly dimmed. So hue = what kind of note, brightness = accented-or-not.
//   A bright vertical tick at a bar's left edge marks each new gate ONSET; MidNote
//   gate-tails draw no bar (the onset's true fractional width already spans them).
//
// ── ENGINE CONTRACT ──────────────────────────────────────────────────────────
// The Lantern is a scope that reads the ENGINE, not cables -- which is why it can show
// per-voice articulation at all (poly exposes only gate+pitch+accent; there is no cable
// carrying "this voice tied"). The cost is coupling: a refactor can silently falsify this
// display, and a faithful scope that has quietly gone stale is worse than none, because it
// is trusted. Audited at 5e76373 (post Rule 2, per-lane direction, stateless positions).
//
// Fields read, and what each must keep meaning:
//   eng.stepIndex                physical playhead column (0..15)
//   eng.lastStepResult.forStep   the stepIndex a decision was computed for. The engine
//                                comments this as "(Lantern pairs decision→column)" -- this
//                                is the pairing.
//   eng.lastStepResult.decision  MONO's decision. Used ONLY for MidNote (tail) detection.
//   eng.lastStepResult.nvIdx     note-length index -> bar length
//   eng.lastStepResult.accented  mono accent
//   eng.lastPlayDir              GLOBAL play direction (+1/-1). Deliberately global: the
//                                columns are physical steps, so bar extension follows the
//                                direction of TIME. Per-lane direction (laneSign_/laneTick_)
//                                is NOT read and must not be -- a reversed lane still played
//                                its notes at the steps shown.
//   eng.numPolyVoices, eng.voices, eng.perVoiceArticulation
//   gs.gateHeld / gs.holdRemain  SOUNDING test, per voice. NOT MonoDecision::Rest -- the
//                                voice's own gate is the per-voice truth.
//   gs.gatePulseRemain           distinguishes a fresh attack from a held-over tail
//   gs.lastNoteType              CELL COLOUR. Single/Tie/Legato.
//   gs.slurForward, pv.accented  per-voice, under perVoiceArticulation
//   gs.slurMember                SLUR UNDERLINE. The SLEG output mask (leads OR continues a
//                                slur), read per voice exactly like lastNoteType. An
//                                underlined run of cells = one fused GATE; its cell edges =
//                                the STEP strikes; the underline = SLEG.
//
// THE INVARIANT THIS DISPLAY RESTS ON:
//   lastNoteType describes the CURRENT NOTE, and a tail is the same note.
// Every path that STARTS a note goes through GateState's five articulation methods
// (triggerNote/slideNote x2/slideMax/extendHold), all of which set it. Four paths in
// SequencerEngine re-assert gateHeld without setting it (~473, 706, 868) -- all are tails of
// a note already sounding, so its type is still correct. Break that (re-open a gate for a
// NEW note without going through those methods) and the Lantern will faithfully draw a stale
// colour. That is the failure mode to watch for.
//
// PURE OBSERVER: reads Monsoon's engine state (via findMonsoonEitherSide) and
// keeps its OWN per-voice 16-step display ring buffer. It writes NO engine state.
// Independent of the Sands topology work. See docs/design/LANTERN_SPEC.md.
// ─────────────────────────────────────────────────────────────────────────────
#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "ui/VisualExpanderHelpers.hpp"   // redDot::findMonsoonEitherSide
#include <string>
#include <algorithm>
#include <cmath>

using namespace rack;

// ── Note-name helper (self-contained; no engine change) ──────────────────────
// 1V/oct → "C4" style label. Rack convention: 0 V = C4.
namespace lantern {
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

// ── Module: display-only params, reads Monsoon read-only ─────────────────────
struct Lantern : Module {
    enum ParamIds {
        VIEW_PARAM,       // 0=Notes 1=Velocity 2=Prob (view-only)
        ZOOM_PARAM,       // 0=x1 1=x2 2=x4
        FOLLOW_PARAM,     // 0/1 keep phase in view
        ROLL_PARAM,       // 0=Grid (lane) view, 1=Piano-roll view
        ROLL_SCROLL_PARAM,// piano-roll vertical scroll: bottom octave of the 5-oct viewport (0..8-5)
        ROLL_COLOR_PARAM, // 0=colour by ROLE (single/tie/legato), 1=colour by VOICE
        NUM_PARAMS
    };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds { NUM_OUTPUTS };
    enum LightIds  { NUM_LIGHTS };

    // Per-voice display ring buffer — Lantern's OWN accumulated history (the
    // engine only keeps lastStepResult, so we record what we observe each step).
    // [voice][step] for the 16 voices × 16 steps.
    struct Cell {
        lantern::NoteType type = lantern::NoteType::Inactive;
        float  pitchV   = 0.f;   // for the label
        float  lengthSteps = 0.f;// TRUE length in 1/16 steps from the gate pulse count
                                 // (gatePulseRemain / pulsesPer16th at this step edge) —
                                 // so triplets/1/32 and tied runs ending on them keep their
                                 // real sub-step end, not the nominal note-value length.
        bool   heldIn   = false; // continues from previous bar
        bool   heldOut  = false; // continues past step 16
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
    Cell cells[16][16];
    int  lastObservedStep = -1;
    int  lastWriteStepObs = -1;   // previous writeStep, for lap-wrap (arrival) detection

    // Piano-roll per-voice visibility (bit v = voice v shown). Default all on. Persisted.
    uint16_t rollVoiceMask = 0xFFFF;
    bool voiceVisible(int v) const { return (rollVoiceMask >> v) & 1u; }
    void toggleVoice(int v)        { rollVoiceMask ^= (uint16_t)(1u << v); }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "rollVoiceMask", json_integer(rollVoiceMask));
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* m = json_object_get(root, "rollVoiceMask"))
            rollVoiceMask = (uint16_t)json_integer_value(m);
    }

    Lantern() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configSwitch(VIEW_PARAM,   0.f, 2.f, 0.f, "View", {"Notes", "Velocity", "Prob"});
        configSwitch(ZOOM_PARAM,   0.f, 2.f, 0.f, "Zoom", {"x1", "x2", "x4"});
        configSwitch(FOLLOW_PARAM, 0.f, 1.f, 1.f, "Follow", {"Off", "On"});
        configSwitch(ROLL_PARAM,   0.f, 1.f, 0.f, "Display", {"Grid", "Piano roll"});
        // Scroll = bottom octave of the 5-octave viewport into the full 0..8 range → range 0..3
        // (bottom oct 0 shows oct 0-4; bottom oct 3 shows oct 3-7... capped so top ≤ 8). Default 2
        // so the viewport (oct 2-6) sits on the default pitch window (lo=2, hi=5).
        configParam(ROLL_SCROLL_PARAM, 0.f, 4.f, 2.f, "Roll scroll (bottom octave)");
        paramQuantities[ROLL_SCROLL_PARAM]->snapEnabled = true;
        configSwitch(ROLL_COLOR_PARAM, 0.f, 2.f, 0.f, "Roll colour", {"By role", "By voice", "By note"});
    }

    // Read-only sampler: records the observed per-step state into Lantern's own
    // display buffer on each step edge. Per-step, as-it-happens, NO look-back — a
    // note reads as Single when it fires and only changes to Tie/Legato at the step
    // where the engine's decision actually becomes a continuation (the 2nd note on).
    // This mirrors what Monsoon really does (probabilistic, not pre-planned), so the
    // colour change AT the join is the truthful tie/legato signal. Trusts MonoDecision.
    // Reads engine state only; writes nothing back.
    void process(const ProcessArgs& args) override {
        Monsoon* mon = redDot::findMonsoonEitherSide(this);
        if (!mon) return;
        SequencerEngine& eng = mon->engine;

        int step = eng.stepIndex;
        if (step == lastObservedStep) return;   // only sample on step edges
        lastObservedStep = step;
        if (step < 0 || step >= 16) return;

        const MonoDecision dec = eng.lastStepResult.decision;
        const bool accentedMono = eng.lastStepResult.accented;
        const float lenSteps = gs_noteSteps(eng.lastStepResult.nvIdx);
        const bool monoSlur = eng.gs.slurForward;   // the mono gate's forward-hold commitment;
                                                     // poly voices follow mono's gate, so inherit it

        // Pair the decision with the column it was actually COMPUTED for. Lantern samples when
        // eng.stepIndex changes, but stepIndex is advanced (by advancePlayhead) BEFORE the
        // engine computes that step's decision into lastStepResult — so at the sample instant
        // stepIndex can already point at the new step while lastStepResult still holds the
        // PREVIOUS step's decision. Writing dec into cells[stepIndex] then paints the wrong
        // column (the previous step's decision lands in the new step's cell → e.g. a real note's
        // cell renders empty/rest). This is direction-sensitive and is the reverse isolated-teal.
        // Fix: write into the decision's OWN step (lastStepResult.forStep), so a decision always
        // lands in its own column regardless of sampling phase.
        int writeStep = (eng.lastStepResult.forStep >= 0 && eng.lastStepResult.forStep < 16)
                        ? eng.lastStepResult.forStep : step;

        // LAP ARRIVAL: the playhead crossed the lap boundary since the last sample — the write
        // step moved AGAINST the travel direction (forward: new < old; reverse: new > old). A
        // note that held its gate OUT of the previous lap is still sounding here, but its onset
        // bar lives on the PREVIOUS lap's cells (clipped at the far edge) and cannot cover this
        // lap's tail — so without special handling the tail renders as nothing (isMidTail skip)
        // and a legato join landing on it looks like teal-after-a-rest. recordCell turns the
        // FIRST tail cell of the lap into a drawable continuation bar instead. (A jump that
        // moves against travel also flags this; its covering onset is likewise elsewhere, so
        // drawing the tail is the truthful render there too.)
        const bool lapArrival = (lastWriteStepObs >= 0) && (eng.lastPlayDir < 0
                                ? (writeStep > lastWriteStepObs)
                                : (writeStep < lastWriteStepObs));
        lastWriteStepObs = writeStep;

        // Row 0 = mono / V1.
        recordCell(0, writeStep, eng.gs, dec, accentedMono, lenSteps, monoSlur, monoSlur, eng.lastPlayDir, lapArrival);

        // Rows 1..numPolyVoices = poly voices 2.. . A poly voice follows mono's
        // gate TYPE (retrigger/tie/legato) but can independently REST (then it's
        // inactive this gate) and draws its OWN accent.
        for (int v = 0; v < eng.numPolyVoices && v < 15; ++v) {
            const PolyVoice& pv = eng.voices[v];
            // With per-voice articulation the voice rolls its OWN forward-slur commitment
            // (Rule 2), so use it; otherwise it follows mono's gate, so inherit monoSlur.
            const bool voiceSlur = eng.perVoiceArticulation ? pv.gs.slurForward : monoSlur;
            // The amber lead marker ties a poly voice's lead to MONO's chain: only show it when
            // mono is also leading (monoSlur). A per-voice slurForward rolled at a mono Tie (where
            // mono is just holding, monoSlur=0) would otherwise draw a lead outline floating outside
            // mono's chain — the "legato lead note outside the mono envelope" symptom. The voice's
            // commitment is unchanged; only the marker is gated.
            recordCell(v + 1, writeStep, pv.gs, dec, pv.accented, lenSteps, voiceSlur, monoSlur, eng.lastPlayDir, lapArrival);
        }
        // Voices beyond numPolyVoices → mark inactive.
        for (int v = eng.numPolyVoices; v < 15; ++v)
            cells[v + 1][writeStep].type = lantern::NoteType::Inactive;
    }

    // Map an observed voice state at a step into a display Cell. NoteType priority:
    // inactive (gate down & not held) → else Accent if accented → else the join type
    // (Tie/Legato) from the decision → else Single (a fresh/normal note).
    // `monoLeading` = mono's slurForward (whether mono is leading a slur). Used to gate the
    // poly lead marker so a per-voice lead never floats outside mono's chain.
    void recordCell(int voice, int step, const GateState& gs, MonoDecision dec,
                    bool accented, float lenSteps, bool slurFwd, bool monoLeading, int playDir,
                    bool lapArrival = false) {
        Cell& c = cells[voice][step];
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
        //       perVoiceArticulation OFF a poly voice follows mono, so slurFwd is mono's
        //       commitment (passed in); with it ON the voice rolls its OWN slurForward
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
        // eng.gs.lastNoteType and is unchanged. (Set by triggerNote/slideNote/extendHold,
        // so it is correct with perVoiceArticulation on OR off.)
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

// ── Display widget: the 16-lane bar render ───────────────────────────────────
struct LanternDisplay : widget::Widget {
    Lantern* module = nullptr;

    // Geometry (mm→px handled by the panel; these are the display-box internals).
    static constexpr int   N_VOICES = 16;
    static constexpr int   N_STEPS  = 16;
    // Layout inside the display box (matches gen_lantern.py): a left gutter for note
    // labels, a right strip for velocity/accent dots, the step grid in between.
    static constexpr float GUTTER_FRAC = 16.f / (208.28f - 12.f);  // ~16mm / DISP_W
    static constexpr float DOTS_FRAC   = 10.f / (208.28f - 12.f);  // ~10mm / DISP_W

    // Text (note-name + row-number labels) MUST be drawn in draw(), NOT drawLayer(1): nvgText
    // glyphs do not composite on Rack's self-illuminating light layer (the bars, being nvgFill
    // rects, do — which is why bars showed but labels never did). Same trap TabButton documents:
    // text renders on the base draw pass, not the glow layer.
    void draw(const DrawArgs& args) override {
        Widget::draw(args);
        if (!module) return;
        NVGcontext* vg = args.vg;
        const float W = box.size.x, H = box.size.y;
        const float laneH = H / N_VOICES;
        const float gutter = W * GUTTER_FRAC;

        // LCD ground: DARK on BOTH themes. The panel's display well follows the host theme
        // (light #d4d6d9 on the light panel), but the cells encode state in colour AND alpha —
        // on a light ground a low-alpha cell washes out and reads as ABSENT, misreporting data
        // (see the note on LanternWidget). So draw our own dark well over the panel's, same rule
        // as SandsVisualEditorV4::setTheme ('theme the panel, leave the screen alone'). Inset a
        // hair so the panel's wellring bezel still frames it — a light bezel round a dark well
        // reads as a recessed screen. Matches gen_lantern.py's dark well (#0f1114).
        {
            const float inset = 1.f;
            nvgBeginPath(vg);
            nvgRoundedRect(vg, inset, inset, W - 2.f * inset, H - 2.f * inset, mm2px(1.2f));
            nvgFillColor(vg, nvgRGB(0x0f, 0x11, 0x14));
            nvgFill(vg);
        }

        auto font0 = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!font0) font0 = APP->window->uiFont;

        // Piano-roll mode: Bitwig-style piano-key gutter. Keys span the FULL gutter width and are
        // drawn first; a SEMI-TRANSPARENT grey label overlay sits on top of the left portion (keys
        // show faintly underneath), with the C-label rows at lower alpha so they read lighter.
        if (module->params[Lantern::ROLL_PARAM].getValue() > 0.5f) {
            if (!font0) return;
            const float rowH   = H / (float)ROLL_ROWS;
            const float gutter = W * GUTTER_FRAC;
            const int   botOct = (int)std::round(module->params[Lantern::ROLL_SCROLL_PARAM].getValue());
            const int   botSemi = botOct * 12;
            auto isBlack = [](int pc){ pc=((pc%12)+12)%12; return pc==1||pc==3||pc==6||pc==8||pc==10; };

            const float keyW   = gutter;                  // keys span the whole gutter width
            const float blackW = keyW * 0.66f;            // black keys ~2/3 width
            const float labelW = gutter * 0.42f;          // grey label zone width

            // 1. White backing across the ENTIRE gutter height — merges E/F, B/C adjacencies and
            //    forms the sliver above/below each black key automatically.
            nvgBeginPath(vg); nvgRect(vg, 0.f, 0.f, keyW, H);
            nvgFillColor(vg, nvgRGB(0xcf, 0xcf, 0xcf)); nvgFill(vg);

            // 2. Black keys — FULL row height, ~2/3 width, on top of the white backing. The white
            //    to the RIGHT of each black key is the notch connecting the white through.
            for (int r = 0; r < ROLL_ROWS; ++r) {
                int pc = ((botSemi + r) % 12 + 12) % 12;
                if (!isBlack(pc)) continue;
                float y = H - (r + 1) * rowH;
                nvgBeginPath(vg);
                nvgRect(vg, 0.f, y, blackW, rowH);
                nvgFillColor(vg, nvgRGB(0x17, 0x17, 0x19)); nvgFill(vg);
            }

            // 3. White-key divider lines. A physical keyboard only has a divider WHERE TWO WHITE KEYS
            //    MEET. In this per-semitone-row layout that is: (a) the CENTRE of each black-key row
            //    (one line splitting the white above from the white below — extends full width into
            //    the white notch, so it reads as a single line dividing the two whites, not a black
            //    edge on both sides of the black key), and (b) the direct white|white boundaries
            //    E|F (pc 4|5) and B|C (pc 11|0).
            nvgStrokeColor(vg, nvgRGBA(0x2a, 0x2a, 0x2e, 0xb0));
            nvgStrokeWidth(vg, 0.75f);
            for (int r = 0; r < ROLL_ROWS; ++r) {
                int pc = ((botSemi + r) % 12 + 12) % 12;
                if (isBlack(pc)) {
                    float yc = H - (r + 0.5f) * rowH;           // centre of the black-key row
                    nvgBeginPath(vg); nvgMoveTo(vg, 0.f, yc); nvgLineTo(vg, keyW, yc); nvgStroke(vg);
                } else if (pc == 5 || pc == 0) {                 // F and C: boundary below them (E|F, B|C)
                    float yb = H - r * rowH;                     // bottom edge of this white row
                    nvgBeginPath(vg); nvgMoveTo(vg, 0.f, yb); nvgLineTo(vg, keyW, yb); nvgStroke(vg);
                }
            }

            // 4. SOUNDING-NOTE key highlight — always FULL row width (a lit black note reads as a
            //    full-width bar like a lit white note, extending over the white notch).
            {
                const int ph = module->lastObservedStep;
                if (ph >= 0 && ph < N_STEPS) {
                    for (int v = 0; v < N_VOICES; ++v) {
                        if (!module->voiceVisible(v)) continue;
                        const auto& c = module->cells[v][ph];
                        if (c.type == lantern::NoteType::Inactive) continue;
                        int semiC0 = (int)std::lround(c.pitchV * 12.f) + 48;
                        int row = semiC0 - botSemi;
                        if (row < 0 || row >= ROLL_ROWS) continue;
                        float y = H - (row + 1) * rowH;
                        NVGcolor hi;
                        {
                            int cm = (int)std::round(module->params[Lantern::ROLL_COLOR_PARAM].getValue());
                            if (cm == 1)      hi = voiceColour(v);
                            else if (cm == 2) hi = noteColour(semiC0);
                            else              hi = typeColour(c.type);
                        }
                        nvgBeginPath(vg);
                        nvgRect(vg, 0.f, y, keyW, rowH);
                        nvgFillColor(vg, nvgTransRGBA(hi, 0xd8)); nvgFill(vg);
                    }
                }
            }

            // 5. Semi-transparent grey LABEL overlay on the left — keys show faintly underneath.
            //    C-label rows use a LOWER alpha (lighter grey) while staying dark enough for the
            //    white text; all other rows a bit more opaque.
            for (int r = 0; r < ROLL_ROWS; ++r) {
                int pc = ((botSemi + r) % 12 + 12) % 12;
                float y = H - (r + 1) * rowH;
                unsigned char a = (pc == 0) ? 0xb0 : 0xd0;   // C rows lighter (more transparent)
                nvgBeginPath(vg);
                nvgRect(vg, 0.f, y, labelW, rowH);
                nvgFillColor(vg, nvgRGBA(0x22, 0x25, 0x2a, a)); nvgFill(vg);
            }

            // 6. Octave labels in the grey zone (white text).
            nvgFontFaceId(vg, font0->handle);
            nvgFontSize(vg, std::min(11.f, rowH * 5.f));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, nvgRGB(0xec, 0xec, 0xf0));
            for (int o = 0; o < ROLL_OCTAVES; ++o) {
                int oct = botOct + o;
                float y = H - (o * 12 + 0.5f) * rowH;
                std::string lbl = std::string("C") + std::to_string(oct);
                nvgText(vg, 2.f, y, lbl.c_str(), nullptr);
            }
            return;
        }
        auto font = font0;
        if (!font) return;

        // Faint lane separators over the dark well, matching gen_lantern.py's dark lanesep
        // (#20242b) so the 16 lanes stay delineated on both themes (the flat well otherwise
        // covered the panel's own separators). Grid view only — the roll view has its own
        // key-row structure. Drawn on the base layer, under the note bars (drawLayer 1).
        {
            const float inset = 1.f;
            nvgStrokeColor(vg, nvgRGB(0x20, 0x24, 0x2b));
            nvgStrokeWidth(vg, 0.75f);
            for (int v = 1; v < N_VOICES; ++v) {
                const float y = v * laneH;
                nvgBeginPath(vg);
                nvgMoveTo(vg, inset, y);
                nvgLineTo(vg, W - inset, y);
                nvgStroke(vg);
            }
        }

        const int ph = module->lastObservedStep;

        for (int v = 0; v < N_VOICES; ++v) {
            const float yc = v * laneH + laneH * 0.5f;

            // Label tracks the note UNDER the playhead: show the pitch of the cell at the current
            // step. If that step is a rest (Inactive) for this voice, fall back to the most recent
            // sounding cell AT OR BEFORE the playhead (the note still ringing / last heard), so the
            // label follows the playhead as it moves instead of showing a static last-note.
            bool anyActive = false; float labelPitch = 0.f;
            if (ph >= 0 && ph < N_STEPS && module->cells[v][ph].type != lantern::NoteType::Inactive) {
                labelPitch = module->cells[v][ph].pitchV; anyActive = true;
            } else if (ph >= 0 && ph < N_STEPS) {
                for (int k = 0; k < N_STEPS; ++k) {
                    int s = ((ph - k) % N_STEPS + N_STEPS) % N_STEPS;  // walk backward from playhead
                    if (module->cells[v][s].type != lantern::NoteType::Inactive) {
                        labelPitch = module->cells[v][s].pitchV; anyActive = true; break;
                    }
                }
            }
            if (!anyActive) continue;

            nvgFontFaceId(vg, font->handle);
            nvgFontSize(vg, std::min(11.f, laneH * 0.62f));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

            nvgFillColor(vg, nvgRGB(0x8a, 0x8f, 0x98));
            std::string num = std::to_string(v + 1);
            nvgText(vg, 2.f, yc, num.c_str(), nullptr);

            nvgFillColor(vg, nvgRGB(0xd8, 0xd8, 0xdc));
            nvgText(vg, gutter * 0.42f, yc, lantern::noteName(labelPitch).c_str(), nullptr);
        }
    }

    // Shared step-block playhead (Sands idiom): translucent column over the whole current step,
    // both edges, and a bright leading-edge bar on the travel-toward side (right fwd / left rev).
    void drawStepPlayhead(NVGcontext* vg, float gridX, float stepW, float H) {
        int ph = module->lastObservedStep;
        if (ph < 0 || ph >= N_STEPS) return;
        int dir = +1;
        for (int v = 0; v < N_VOICES; ++v) {
            if (module->cells[v][ph].type != lantern::NoteType::Inactive) { dir = module->cells[v][ph].playDir; break; }
        }
        float cx = gridX + ph * stepW;
        nvgBeginPath(vg);
        nvgRect(vg, cx, 0.f, stepW, H);
        nvgFillColor(vg, nvgRGBA(0xd4, 0x00, 0x1a, 0x22));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgStrokeColor(vg, nvgRGBA(0xd4, 0x00, 0x1a, 0x55));
        nvgStrokeWidth(vg, 0.8f);
        nvgMoveTo(vg, cx, 0.f);         nvgLineTo(vg, cx, H);
        nvgMoveTo(vg, cx + stepW, 0.f); nvgLineTo(vg, cx + stepW, H);
        nvgStroke(vg);
        float ex = (dir < 0) ? cx : (cx + stepW);
        nvgBeginPath(vg);
        nvgStrokeColor(vg, nvgRGBA(0xd4, 0x00, 0x1a, 0xdd));
        nvgStrokeWidth(vg, 1.6f);
        nvgMoveTo(vg, ex, 0.f); nvgLineTo(vg, ex, H);
        nvgStroke(vg);
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) { Widget::drawLayer(args, layer); return; }
        if (!module) return;

        // Piano-roll view replaces the lane grid on this layer.
        if (module->params[Lantern::ROLL_PARAM].getValue() > 0.5f) { drawRollLayer(args); return; }

        NVGcontext* vg = args.vg;
        const float W = box.size.x, H = box.size.y;
        const float laneH = H / N_VOICES;
        const float gutter = W * GUTTER_FRAC;
        const float dots   = W * DOTS_FRAC;
        const float gridX  = gutter;
        const float gridW  = W - gutter - dots;
        const float stepW  = gridW / N_STEPS;

        for (int v = 0; v < N_VOICES; ++v) {
            const float y0 = v * laneH;
            const float yc = y0 + laneH * 0.5f;
            const float barH = laneH * 0.62f;

            // (Note-name / row labels are drawn in draw(), not here — nvgText doesn't
            //  composite on the light layer. See draw() above.)

            // per-step bars
            for (int s = 0; s < N_STEPS; ++s) {
                const auto& c = module->cells[v][s];
                if (c.type == lantern::NoteType::Inactive) continue;
                // Held-out caret is drawn BEFORE the MidNote-tail skip below, because a long
                // slur-lead note often TAILS through the last step (its onset was earlier), and
                // that tail is a MidNote which the skip would otherwise drop — losing the caret.
                // (yc/barH are per-lane, available here.)
                // Held-out caret = the note's gate LEAVES the pattern past the edge it travels
                // TOWARD: forward that's the RIGHT edge (step 15); reverse the LEFT edge (step 0).
                // The arrow points outward in the travel direction. Independent of held-in (a note
                // can hold OUT without the next lap holding IN — a slur-lead whose landing is a
                // fresh note), so this fires on its own condition at its own edge.
                {
                    const bool rev = (c.playDir < 0);
                    const int  outStep = rev ? 0 : (N_STEPS - 1);
                    if (s == outStep && c.heldOut) {
                        const float ex = rev ? gridX : (gridX + gridW);   // leaving edge
                        const float tip = rev ? (ex - 3.f) : (ex + 3.f);  // arrow points outward
                        nvgBeginPath(vg);
                        nvgMoveTo(vg, tip, yc);
                        nvgLineTo(vg, ex,  yc - barH * 0.4f);
                        nvgLineTo(vg, ex,  yc + barH * 0.4f);
                        nvgClosePath(vg);
                        nvgFillColor(vg, nvgRGB(0xc8, 0x96, 0x0c));
                        nvgFill(vg);
                    }
                }
                // Skip MidNote gate-tails: the onset (or tie-join) cell's true fractional
                // width already draws through this step and stops where the gate closes,
                // leaving the real sub-step gap (triplet/1/32). Drawing a bar here would
                // fill that gap and make a 1/8T look like a straight 1/8.
                if (c.isMidTail) continue;
                const float span = std::max(0.15f, c.lengthSteps <= 0.f ? 1.f : c.lengthSteps);
                float bx, bw;
                if (c.playDir < 0) {
                    // Reverse: anchor the bar to the RIGHT edge of the cell and extend LEFT, so a
                    // short note's fill reaches toward the note it connects FROM (its predecessor
                    // sits to the right in reverse play). Left-anchoring here would leave the
                    // note's empty right portion as a gap on the play-direction side — the
                    // "isolated teal with a rest to its right" the engine never actually produced.
                    const float cellRight = gridX + (s + 1) * stepW;
                    bw = std::min(span * stepW, cellRight - gridX);
                    bx = cellRight - bw;
                } else {
                    // Forward: anchor left, extend right (unchanged).
                    bx = gridX + s * stepW;
                    bw = std::min(span * stepW, (gridX + gridW) - bx);
                }

                int cm = (int)std::round(module->params[Lantern::ROLL_COLOR_PARAM].getValue());
                NVGcolor col = (cm == 2)
                    ? noteColour((int)std::lround(c.pitchV * 12.f) + 48)
                    : typeColour(c.type);
                // accent = shade: brighter when accented, slightly dimmed when not
                if (c.accented) col = nvgLerpRGBA(col, nvgRGB(0xff, 0xff, 0xff), 0.28f);
                else            col = nvgLerpRGBA(col, nvgRGB(0x00, 0x00, 0x00), 0.10f);

                nvgBeginPath(vg);
                nvgRoundedRect(vg, bx + 0.5f, yc - barH * 0.5f, std::max(1.5f, bw - 1.f), barH, 1.5f);
                nvgFillColor(vg, col);
                nvgFill(vg);

                // Legato-LEAD marker: a note that initiates a slur gets a distinct outline
                // (amber -- the CONNECTION-family hue shared with the wraparound carets and
                // the slur underline; NOT the accent hue, which is the brand-red top edge)
                // drawn OVER its normal fill, so the lead intent is visible at the chain's
                // start regardless of the note's base colour. Non-destructive: fill colour is
                // unchanged.
                if (c.leadsLegato) {
                    nvgBeginPath(vg);
                    nvgRoundedRect(vg, bx + 1.0f, yc - barH * 0.5f + 0.5f,
                                   std::max(1.0f, bw - 2.0f), barH - 1.0f, 1.5f);
                    nvgStrokeColor(vg, nvgRGB(0xe0, 0xa8, 0x1c));   // amber lead outline
                    nvgStrokeWidth(vg, 1.4f);
                    nvgStroke(vg);
                }

                // SLUR UNDERLINE (SLEG): a thin amber bar along the bottom edge of every cell
                // whose note is a slur member — musical notation's slur arc, flattened for an
                // LED grid. An underlined RUN spans exactly one fused GATE (the lead's amber
                // outline marks where it starts); the cell edges inside the run are the STEP
                // strikes; the underline itself is the SLEG mask. Same amber family as the
                // lead outline so all slur semantics share one hue. Non-destructive overlay.
                if (c.slurMember) {
                    nvgBeginPath(vg);
                    nvgRect(vg, bx + 0.5f, yc + barH * 0.5f + 1.0f,
                            std::max(1.5f, bw - 1.f), 1.5f);
                    nvgFillColor(vg, nvgRGB(0xe0, 0xa8, 0x1c));
                    nvgFill(vg);
                }


                // step, 1/32 → tick mid-step, etc). Without this, runs of long notes merge
                // into one solid line. A Tie/Legato continuation gets NO tick (it's held).
                const bool onset = !c.heldIn;
                if (onset) {
                    NVGcolor tick = nvgLerpRGBA(col, nvgRGB(0xff, 0xff, 0xff), 0.55f);
                    nvgBeginPath(vg);
                    nvgRect(vg, bx + 0.5f, yc - barH * 0.5f, 1.4f, barH);
                    nvgFillColor(vg, tick);
                    nvgFill(vg);
                }

                // accented notes get a thin brand-red top edge for extra legibility
                if (c.accented) {
                    nvgBeginPath(vg);
                    nvgRect(vg, bx + 0.5f, yc - barH * 0.5f, std::max(1.5f, bw - 1.f), 1.2f);
                    nvgFillColor(vg, accentColour());
                    nvgFill(vg);
                }
                // Held-in caret = the note's gate ARRIVES from before the pattern, at the edge it
                // travels FROM: forward the LEFT edge (step 0); reverse the RIGHT edge (step 15).
                // The arrow points INWARD (into the grid, along the travel direction). Independent
                // of held-out — mirrors it by play direction.
                {
                    const bool rev = (c.playDir < 0);
                    const int  inStep = rev ? (N_STEPS - 1) : 0;
                    if (s == inStep && c.heldIn) {
                        const float ex  = rev ? (gridX + gridW) : gridX;   // arriving edge
                        const float tip = rev ? (ex + 3.f) : (ex - 3.f);   // arrow points outward from edge
                        nvgBeginPath(vg);
                        nvgMoveTo(vg, tip, yc);
                        nvgLineTo(vg, ex,  yc - barH * 0.4f);
                        nvgLineTo(vg, ex,  yc + barH * 0.4f);
                        nvgClosePath(vg);
                        nvgFillColor(vg, nvgRGB(0xc8, 0x96, 0x0c));
                        nvgFill(vg);
                    }
                }
                // held-out caret: drawn earlier (before the MidNote-tail skip) so it shows
                // even when the last step is a tail of a long slur-lead note.
            }
        }

        // current-step block highlight (shared with the roll)
        drawStepPlayhead(vg, gridX, stepW, H);
    }

    // ── Piano-roll view ──────────────────────────────────────────────────────
    // A 5-octave viewport onto the full 0..8 octave range; ROLL_SCROLL_PARAM = the bottom octave
    // of the window. Notes draw as horizontal bars at their pitch row, spanning lengthSteps.
    // Colour: by ROLE (note type) or by VOICE (per-voice hue). Lead outline kept on top.
    static constexpr int ROLL_OCTAVES = 4;                 // viewport height in octaves (taller rows)
    static constexpr int ROLL_ROWS    = ROLL_OCTAVES * 12; // = 48 semitone rows

    // Distinct, legible-on-dark hue per voice (mono = v0). Kept modest saturation so overlaps read.
    static NVGcolor voiceColour(int v) {
        static const NVGcolor P[8] = {
            nvgRGB(0x6c, 0x8c, 0xd4), // 1 blue
            nvgRGB(0x26, 0xa6, 0x9a), // 2 teal
            nvgRGB(0xd4, 0x8a, 0x3c), // 3 amber
            nvgRGB(0xb0, 0x6c, 0xd4), // 4 violet
            nvgRGB(0x5c, 0xb8, 0x5c), // 5 green
            nvgRGB(0xd4, 0x6c, 0x8c), // 6 rose
            nvgRGB(0x4c, 0xb0, 0xc8), // 7 cyan
            nvgRGB(0xc8, 0xb0, 0x4c), // 8 gold
        };
        return P[((v % 8) + 8) % 8];
    }

    // Distinct hue per pitch class (C=0..B=11), aligned with Bitwig Studio's
    // piano-roll "by pitch" colour scheme. Same colour for the same note name
    // across all octaves, so you can see harmonic content at a glance.
    static NVGcolor noteColour(int pitchClass) {
        static const NVGcolor P[12] = {
            nvgRGB(0xef, 0x53, 0x50), // C  — red
            nvgRGB(0xff, 0x70, 0x43), // C# — deep orange
            nvgRGB(0xff, 0xa7, 0x26), // D  — orange
            nvgRGB(0xff, 0xca, 0x28), // D# — amber
            nvgRGB(0xd4, 0xe1, 0x57), // E  — lime
            nvgRGB(0x66, 0xbb, 0x6a), // F  — green
            nvgRGB(0x26, 0xa6, 0x9a), // F# — teal
            nvgRGB(0x29, 0xb6, 0xf6), // G  — light blue
            nvgRGB(0x42, 0xa5, 0xf5), // G# — blue
            nvgRGB(0x5c, 0x6b, 0xc0), // A  — indigo
            nvgRGB(0xab, 0x47, 0xbc), // A# — purple
            nvgRGB(0xec, 0x40, 0x7a), // B  — pink
        };
        return P[((pitchClass % 12) + 12) % 12];
    }

    // Voices that have any note this pattern (mono=0 + active poly) — the legend/toggle set.
    int rollActiveVoiceCount() const {
        int n = 1;  // mono always present
        for (int v = 1; v < N_VOICES; ++v)
            for (int s = 0; s < N_STEPS; ++s)
                if (module->cells[v][s].type != lantern::NoteType::Inactive) { if (v + 1 > n) n = v + 1; break; }
        return n;
    }
    // Rect of voice v's legend swatch (top strip). Also the click target.
    rack::Rect voiceSwatchRect(int v, int count) const {
        const float W = box.size.x;
        const float gutter = W * GUTTER_FRAC;
        const float gridW  = W - gutter - (W * DOTS_FRAC);
        const float sw = std::min(16.f, gridW / std::max(1, count));
        return rack::Rect(gutter + v * sw + 1.f, 1.f, sw - 2.f, 9.f);
    }

    void drawRollLayer(const DrawArgs& args) {
        NVGcontext* vg = args.vg;
        const float W = box.size.x, H = box.size.y;
        const float gutter = W * GUTTER_FRAC;
        const float gridX  = gutter;
        const float gridW  = W - gutter - (W * DOTS_FRAC);
        const float stepW  = gridW / N_STEPS;
        const float rowH   = H / (float)ROLL_ROWS;

        const int   botOct   = (int)std::round(module->params[Lantern::ROLL_SCROLL_PARAM].getValue());
        const int   botSemi  = botOct * 12;          // semitone-from-C0 at the bottom row
        const int   colorMode = (int)std::round(module->params[Lantern::ROLL_COLOR_PARAM].getValue());

        // ── Bitwig-style lane background: shade each semitone row by pitch class. ──
        // Black-key rows (C#,D#,F#,G#,A#) get a darker slate; white-key rows a lighter blue-grey.
        // This alternating banding is what makes the grid read as a piano roll at a glance.
        auto isBlackKey = [](int pc) {
            pc = ((pc % 12) + 12) % 12;
            return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        };
        for (int r = 0; r < ROLL_ROWS; ++r) {
            int pc = ((botSemi + r) % 12 + 12) % 12;
            float y = H - (r + 1) * rowH;
            // white-key lane = lighter blue-grey; black-key lane = darker slate.
            NVGcolor lane = isBlackKey(pc) ? nvgRGB(0x20, 0x2a, 0x33) : nvgRGB(0x2b, 0x38, 0x44);
            // Subtle octave emphasis: the C row (pitch class 0) a touch brighter so octaves anchor.
            if (pc == 0) lane = nvgRGB(0x32, 0x40, 0x4d);
            nvgBeginPath(vg);
            nvgRect(vg, gridX, y, gridW, rowH);
            nvgFillColor(vg, lane);
            nvgFill(vg);
        }

        // Faint vertical beat grid (bar + beat divisions), behind the notes.
        {
            const float barW = gridW / 4.f;             // N_STEPS = 16 = one 4/4 bar of 1/16s
            for (int b = 0; b <= 16; ++b) {
                float x = gridX + b * stepW;
                bool beat = (b % 4 == 0);
                nvgBeginPath(vg);
                nvgStrokeColor(vg, beat ? nvgRGBA(0x50, 0x58, 0x64, 0xa0)
                                        : nvgRGBA(0x3c, 0x44, 0x4e, 0x60));
                nvgStrokeWidth(vg, beat ? 1.0f : 0.5f);
                nvgMoveTo(vg, x, 0.f); nvgLineTo(vg, x, H);
                nvgStroke(vg);
            }
            (void)barW;
        }
        // Horizontal separators — very subtle now that the lane SHADING provides the banding.
        // Just a faint line at each C (octave) for reference; semitone lines dropped (the
        // per-row shading already gives pitch reference, and Bitwig keeps these minimal).
        for (int o = 0; o <= ROLL_OCTAVES; ++o) {
            float y = H - o * 12 * rowH;
            nvgBeginPath(vg);
            nvgStrokeColor(vg, nvgRGBA(0x50, 0x58, 0x64, 0x70));   // faint C line
            nvgStrokeWidth(vg, 0.75f);
            nvgMoveTo(vg, gridX, y); nvgLineTo(vg, gridX + gridW, y);
            nvgStroke(vg);
        }

        // Notes.
        for (int v = 0; v < N_VOICES; ++v) {
            if (!module->voiceVisible(v)) continue;   // per-voice visibility toggle
            for (int s = 0; s < N_STEPS; ++s) {
                const auto& c = module->cells[v][s];
                if (c.type == lantern::NoteType::Inactive) continue;
                if (c.isMidTail) continue;   // tail of a longer note; its onset cell draws the bar

                // pitchV (0V=C4) → semitone-from-C0 = round(pitchV*12) + 48.
                int semiC0 = (int)std::lround(c.pitchV * 12.f) + 48;
                int row = semiC0 - botSemi;                 // 0 = bottom viewport row
                if (row < 0 || row >= ROLL_ROWS) continue;  // outside the scrolled window

                float y  = H - (row + 1) * rowH;            // row 0 at the bottom
                float bx = gridX + s * stepW;
                float span = std::max(0.25f, c.lengthSteps <= 0.f ? 1.f : c.lengthSteps);
                float bw = std::min(span * stepW, (gridX + gridW) - bx);

                NVGcolor col;
                if (colorMode == 1)      col = voiceColour(v);
                else if (colorMode == 2) col = noteColour(semiC0);
                else                     col = typeColour(c.type);
                if (c.accented) col = nvgLerpRGBA(col, nvgRGB(0xff,0xff,0xff), 0.28f);

                nvgBeginPath(vg);
                nvgRoundedRect(vg, bx + 0.5f, y + 0.5f, std::max(1.5f, bw - 1.f),
                               std::max(1.5f, rowH - 1.f), 1.f);
                nvgFillColor(vg, col);
                nvgFill(vg);

                if (c.leadsLegato) {  // amber lead outline, on top, both colour modes
                    nvgBeginPath(vg);
                    nvgRoundedRect(vg, bx + 1.f, y + 1.f, std::max(1.f, bw - 2.f),
                                   std::max(1.f, rowH - 2.f), 1.f);
                    nvgStrokeColor(vg, nvgRGB(0xe0, 0xa8, 0x1c));
                    nvgStrokeWidth(vg, 1.2f);
                    nvgStroke(vg);
                }
                if (c.slurMember) {  // SLEG underline (see grid view) — under the note's bottom edge
                    nvgBeginPath(vg);
                    nvgRect(vg, bx + 0.5f, y + std::max(1.5f, rowH - 1.f) - 0.75f,
                            std::max(1.5f, bw - 1.f), 1.25f);
                    nvgFillColor(vg, nvgRGB(0xe0, 0xa8, 0x1c));
                    nvgFill(vg);
                }
            }
        }

        // Voice legend / toggles: a row of swatches at the top. Filled when the voice is visible,
        // hollow (outline only) when hidden. Click to toggle (see onButton).
        {
            int count = rollActiveVoiceCount();
            for (int v = 0; v < count; ++v) {
                rack::Rect r = voiceSwatchRect(v, count);
                NVGcolor col = voiceColour(v);
                nvgBeginPath(vg);
                nvgRoundedRect(vg, r.pos.x, r.pos.y, r.size.x, r.size.y, 1.5f);
                if (module->voiceVisible(v)) {
                    nvgFillColor(vg, col);
                    nvgFill(vg);
                } else {
                    nvgStrokeColor(vg, nvgTransRGBA(col, 0x90));
                    nvgStrokeWidth(vg, 1.f);
                    nvgStroke(vg);
                }
            }
        }

        // current-step block highlight (shared with the grid)
        drawStepPlayhead(vg, gridX, stepW, H);
    }

    // Click a legend swatch (roll mode) to toggle that voice's visibility.
    void onButton(const event::Button& e) override {
        if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT
            && module->params[Lantern::ROLL_PARAM].getValue() > 0.5f) {
            int count = rollActiveVoiceCount();
            for (int v = 0; v < count; ++v) {
                if (voiceSwatchRect(v, count).contains(e.pos)) {
                    module->toggleVoice(v);
                    e.consume(this);
                    return;
                }
            }
        }
        Widget::onButton(e);
    }

    // Base colour per note type. Accent is drawn as a separate overlay (brighter
    // outline / brand-red marker) on top of these, so an accented note keeps its
    // tie/legato/single identity.
    static NVGcolor typeColour(lantern::NoteType t) {
        switch (t) {
            case lantern::NoteType::Single: return nvgRGB(0x6c, 0x8c, 0xd4);  // calm blue
            case lantern::NoteType::Tie:    return nvgRGB(0x8a, 0x6c, 0xd4);  // violet — held/joined
            case lantern::NoteType::Legato: return nvgRGB(0x26, 0xa6, 0x9a);  // teal — slid
            default:                        return nvgRGB(0x3a, 0x40, 0x48);  // inactive
        }
    }
    static NVGcolor accentColour() { return nvgRGB(0xd4, 0x00, 0x1a); }       // Singapore red overlay
};

// ── Module widget ────────────────────────────────────────────────────────────
struct LanternWidget : ModuleWidget {
    // Panel theme. Lantern_panel_light.svg has existed and shipped all along -- this widget
    // simply never used it, hard-loading the dark panel unconditionally. Same swap as the
    // Sands expanders (StraitsEastSandsVisual): follow the connected Monsoon, so one toggle
    // on Monsoon themes the whole suite.
    //
    // The swap is trivially safe here because the panel carries NO text: both SVGs are 19
    // lines / 8 rects / 6 circles and nothing else, and every nvgText in this file is inside
    // LanternDisplay, i.e. on the LCD. So no label can be left in the wrong colour -- unlike
    // Monsoon, where panel text is drawn at runtime and that split is the root of the
    // outstanding label work.
    //
    // (Corollary worth fixing separately: the panel has no silkscreen at all. VIEW / ZOOM /
    //  FOLLOW / DISPLAY are named via configSwitch so they tooltip, but nothing is printed
    //  beside the controls.)
    //
    // The LCD is deliberately NOT themed. LanternDisplay keeps its dark ground on both
    // themes because its cell colours are high-luminance and tuned against dark, and -- the
    // real reason -- the grid encodes state in colour AND alpha, so on a light ground a
    // low-alpha cell washes out and reads as ABSENT rather than quiet. That would make the
    // display misreport the data. Theme the panel, leave the screen alone (same rule as
    // SandsVisualEditorV4::setTheme).
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;   // -1 = unset, forces the first apply

    LanternWidget(Lantern* module) {
        setModule(module);
        // Panel SVGs generated by panel_src/gen_lantern.py.
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Lantern_panel_dark.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Lantern_panel_light.svg"));
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Lantern_panel_dark.svg")));

        // Screws (C++ RedScrew, matching the family).
        addChild(createWidget<redDot::RedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<redDot::RedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<redDot::RedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<redDot::RedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // The display box mirrors gen_lantern.py: DISP_X=6, DISP_Y=16, DISP_W=W-12,
        // DISP_H=96 mm on a 41HP (208.28mm) panel. The widget reserves a left gutter
        // (~16mm) for note-name labels and a right strip (~10mm) for velocity/accent
        // dots; the 16 lanes fill the height (LANE_H = 96/16 = 6mm).
        auto* disp = new LanternDisplay();
        disp->module = module;
        disp->box.pos  = mm2px(Vec(6.f, 16.f));
        disp->box.size = mm2px(Vec(208.28f - 12.f, 96.f));
        addChild(disp);

        // Controls along the bottom strip — positions mirror gen_lantern.py wells
        // (View buttons x≈10/33/56, Zoom knob centre, Follow x≈W-30, all near y≈118mm).
        // Display-only params; a Trimpot per control (View 0-2, Zoom 0-2, Follow 0-1).
        // TL1105 momentary buttons could replace View/Follow later for nicer UX.
        addParam(createParamCentered<Trimpot>(
            mm2px(Vec(20.f, 118.f)), module, Lantern::VIEW_PARAM));
        addParam(createParamCentered<Trimpot>(
            mm2px(Vec(208.28f / 2.f, 118.f)), module, Lantern::ZOOM_PARAM));
        addParam(createParamCentered<Trimpot>(
            mm2px(Vec(208.28f - 20.f, 118.f)), module, Lantern::FOLLOW_PARAM));

        // Piano-roll controls: Grid/Roll toggle, vertical scroll, colour mode.
        addParam(createParamCentered<Trimpot>(
            mm2px(Vec(45.f, 118.f)), module, Lantern::ROLL_PARAM));
        addParam(createParamCentered<Trimpot>(
            mm2px(Vec(70.f, 118.f)), module, Lantern::ROLL_SCROLL_PARAM));
        addParam(createParamCentered<Trimpot>(
            mm2px(Vec(95.f, 118.f)), module, Lantern::ROLL_COLOR_PARAM));
    }
    void step() override {
        ModuleWidget::step();
        if (!module) return;
        // Follow the connected Monsoon. If none is attached, hold the last theme rather than
        // snapping to dark -- matches the Sands expanders, which return early without a host.
        Monsoon* mon = redDot::findMonsoonEitherSide(module);
        if (!mon) return;
        const int wantLight = mon->lightTheme ? 1 : 0;
        if (wantLight == lastThemeLight) return;
        lastThemeLight = wantLight;
        for (Widget* child : children) {
            if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                sp->setBackground(wantLight ? panelSvgLight : panelSvgDark);
                break;
            }
        }
        // NB no display retheme here, by design -- see the note at the top of this struct.
    }
};

Model* modelLantern = createModel<Lantern, LanternWidget>("Lantern");
