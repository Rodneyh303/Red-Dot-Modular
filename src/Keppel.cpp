#include <rack.hpp>
#include <cmath>
#include <vector>
#include "Monsoon.hpp"                    // pluginInstance
#include "ui/SvgPanelKit.hpp"
#include "dsp/MpeMath.hpp"
#include "dsp/MpeTransmitter.hpp"

using namespace rack;

// ── Keppel — poly microtonal CV → MPE MIDI OUT (MPE_UTILITY_BUILD_SPEC / MICROTONAL_MIDI_MPE_DIRECTION).
// Splits each poly voice into nearest-12-TET note + per-note pitch bend, one MPE member channel per
// voice, so poly microtonal patterns play out to a DAW / MPE synth with the tuning intact. STANDALONE
// utility: two poly cables in (1V/oct pitch + gate), MIDI out. ZERO engine/TuningTable coupling — the
// microtonal-ness is already in the voltage (dotModular::mpe does the exact note/bend split).
//
// The SDK's dsp::MidiGenerator is single-zone (no per-message channel; one global bend), so we can't use
// it for MPE. Instead a thin router builds each midi::Message with setChannel(memberCh) and sends via
// midi::Output directly (Port::channel left at -1 so it doesn't overwrite our channel). ──────────────

extern Model* modelKeppel;

namespace KeppelIds {
    enum ParamIds { BEND_RANGE_PARAM, NUM_PARAMS };
    // Two-layer model (MPE_UTILITY_BUILD_SPEC "Two-layer input structure"): each expression dimension has
    // an A jack (main-gated base) + a B jack (accent-gated additive). Existing PITCH/GATE/ACCENT/VEL are
    // KEPT UNCHANGED. The 8 NEW jacks: X_A/X_B, Y_A/Y_B, Z_A/Z_B, VEL_B, STEP_GATE. VEL_INPUT is velocity's
    // layer A. 13 inputs total.
    enum InputIds {
        PITCH_INPUT, GATE_INPUT, ACCENT_INPUT, VEL_INPUT,   // existing (unchanged)
        X_A_INPUT, X_B_INPUT,                               // X: expressive pitch bend (added after quantiser)
        Y_A_INPUT, Y_B_INPUT,                               // Y: CC74 timbre (bipolar around 64)
        Z_A_INPUT, Z_B_INPUT,                               // Z: channel pressure (unipolar from 0)
        VEL_B_INPUT,                                        // velocity layer B (accent-gated additive)
        STEP_GATE_INPUT,                                    // within-legato (step) gate — inner note divisions
        NUM_INPUTS
    };
    enum OutputIds { MONITOR_OUTPUT, NUM_OUTPUTS };
    enum LightIds  { ACTIVE_LIGHT, NUM_LIGHTS };
}

struct Keppel : Module {
    midi::Output midiOut;

    // The generic CV→MPE-out transmitter (Layer 1 of the separation-of-concerns): member-channel
    // allocation (LRU), the MPE lower-zone config handshake, the per-voice note lifecycle (incl. release
    // velocity + re-articulation), and continuous bend/CC74/pressure with dedupe. Rack-agnostic and
    // unit-tested (test/test_MpeTransmitter.cpp). THIS module is the FACE: it reads CV, does the
    // microtonal split + the A/B expression window + the accent defaults, and feeds tx computed
    // MIDI-domain values. Member-zone size + handshake state live on tx (tx.memberCount, tx.needHandshake).
    dotModular::mpe::MpeTransmitter tx;
    static constexpr int MAX_MEMBERS = dotModular::mpe::MpeTransmitter::MAX_MEMBERS;

    // Two-level velocity from the ACCENT input (dot.modular's accent is a 10V/0V gate). Sampled per
    // voice at note-on: accent-gate high → velAccent, else velNormal. Unpatched ACCENT → every note
    // uses velNormal (fully back-compatible). Both menu-adjustable (receivers vary in velocity curve).
    int   velNormal = 80;
    int   velAccent = 127;
    // Opinionated default for the Y (CC74) brighten on accent when Y_B is unpatched (MPE_UTILITY_BUILD_SPEC
    // "Two-layer": defaults apply ONLY when a B jack is unpatched — velocity boost + Y brighten on accent,
    // X and Z silent). +32 CC74 ≈ half-range toward open/bright.
    int   yAccentBrighten = 32;

    // Expressive X bend past ±bendRange during a HELD gate: CLAMP (default) vs re-articulate.
    //   - CLAMP (default): the bend rails at ±bendRange; the note stays alive, the slur/envelope is
    //     preserved, and there is no boundary chatter. The cost is reach — the pitch ceilings at the
    //     rail and the top of a big slide is lost (visible on MONITOR). A held gate means "sustain this
    //     note", so a silent re-attack mid-gesture is the wrong default; widen the range for more reach.
    //     The microtonal residual is ≤0.5 st, so with range ≥2 clamp NEVER mistunes the tuning content —
    //     it only ever caps the expressive X.
    //   - re-articulate (menu opt-in): re-note on the same member channel so the pitch tracks past the
    //     range, at the cost of a retrigger (breaks the slur). For players who specifically want
    //     unlimited glide reach.
    // NOTE: this flag gates ONLY the over-range path. The within-legato STEP_GATE re-articulation is a
    // separate, deliberate trigger (inner-note boundaries) and always fires regardless of this flag.
    bool  reArticulateOnExceed = false;

    // Edge-detection state for THIS module's gate inputs (the note/channel/dedupe state lives on tx).
    // gatePrev = previous-block main gate; stepGatePrev = previous-block within-legato (step) gate.
    bool gatePrev[16]     = {};
    bool stepGatePrev[16] = {};

    Keppel() {
        using namespace KeppelIds;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(BEND_RANGE_PARAM, 1.f, 48.f, 2.f, "Pitch-bend range", " semitones");
        paramQuantities[BEND_RANGE_PARAM]->snapEnabled = true;
        configInput(PITCH_INPUT,  "Poly pitch (1V/oct)");
        configInput(GATE_INPUT,   "Poly gate (main gate — note on/off + legato envelope)");
        configInput(ACCENT_INPUT, "Poly accent gate (→ note-on velocity + B-layer defaults; unpatched = normal)");
        configInput(VEL_INPUT,    "Poly velocity CV (layer A; 0–10V → 1–127; overrides accent when patched)");
        // Two-layer expression jacks (MPE_UTILITY_BUILD_SPEC "Two-layer input structure"). A = main-gated
        // base (alive the whole note); B = accent-gated additive (windowed inside a live note).
        configInput(X_A_INPUT, "Poly X-A expr (1V=1st bend, bipolar @0; added after note+residual split)");
        configInput(X_B_INPUT, "Poly X-B expr (accent-gated additive bend; unpatched = silent)");
        configInput(Y_A_INPUT, "Poly Y-A expr (CC74 timbre, bipolar @64; ±5V→0..127)");
        configInput(Y_B_INPUT, "Poly Y-B expr (accent-gated additive CC74; unpatched = accent brighten)");
        configInput(Z_A_INPUT, "Poly Z-A expr (channel pressure, unipolar @0; 10V→127)");
        configInput(Z_B_INPUT, "Poly Z-B expr (accent-gated additive pressure; unpatched = silent)");
        configInput(VEL_B_INPUT, "Poly velocity-B CV (accent-gated additive; 0–10V→0-127; unpatched = accent boost)");
        configInput(STEP_GATE_INPUT, "Poly within-legato (step) gate — inner note divisions / forced re-artic grid");
        configOutput(MONITOR_OUTPUT, "Reverse-calc monitor: reconstructed pitch CV (scope vs PITCH in)");
        midiOut.channel = -1;     // we set each message's channel ourselves (per-voice MPE)
        // CV→MPE: the input poly-voice index is meaningful, so default to a STABLE voice→channel mapping
        // (voice v → member v+1). Keeps a voice on one MIDI channel over time — a Keppel→MPE-in loop then
        // lands on the same voice. Menu switches to Dynamic (LRU) for a real MPE synth / stealing.
        tx.alloc = dotModular::mpe::MpeTransmitter::Alloc::Fixed;
    }

    // ── drain the transmitter's outbox into the MIDI port (RawMsg → midi::Message). The core builds the
    // exact wire bytes; this just carries them out. 2-byte messages (channel pressure) skip byte 2 so no
    // stray third byte reaches picky MPE receivers. ────────────────────────────────────────────────────
    void drain() {
        for (const auto& msg : tx.outbox) {
            midi::Message m;
            m.setSize(msg.size);
            m.setStatus(msg.status);
            m.setChannel(msg.channel);
            m.bytes[1] = msg.d1;
            if (msg.size >= 3) m.bytes[2] = msg.d2;
            m.setFrame(msg.frame);
            midiOut.sendMessage(m);
        }
        tx.outbox.clear();
    }

    // Panic / teardown (off the audio thread — menu, reset, patch load): queue note-offs for everything
    // sounding and flush immediately. Clears any pending outbox first so nothing is double-sent.
    void panic() { tx.outbox.clear(); tx.allNotesOff(-1); drain(); }

    void onReset() override {
        panic();
        tx.needHandshake = true;
        tx.lastBendRange = -1;
    }

    void process(const ProcessArgs& args) override {
        using namespace KeppelIds;
        // MPE requires per-message channels: keep the Port from force-overwriting them. The on-panel
        // MidiDisplay channel row can set this live, so re-assert every block (it self-corrects to
        // "All channels"). Driver/device selection on the display is unaffected.
        midiOut.channel = -1;
        const int bendRange = (int)std::round(params[BEND_RANGE_PARAM].getValue());

        // Clears the outbox for this block and (re)sends the MPE config on init / member-count change /
        // bend-range change. We drain the outbox into midiOut at the end of the block.
        tx.beginBlock(bendRange, args.frame);

        const int channels = inputs[PITCH_INPUT].getChannels();
        outputs[MONITOR_OUTPUT].setChannels(channels);
        bool anyActive = false;

        // Helper: read a poly CV channel, returning 0V (the dimension rest for all three of X/Y/Z) when
        // the jack is unpatched or the channel is absent. An unpatched A or B layer therefore contributes
        // the rest point — patchable-not-menued by construction.
        auto readPoly = [&](int id, int voice) -> float {
            auto& in = inputs[id];
            return (in.isConnected() && in.getChannels() > voice) ? in.getVoltage(voice) : 0.f;
        };
        auto clamp127 = [](int x, int lo) { return x < lo ? lo : (x > 127 ? 127 : x); };

        for (int v = 0; v < 16; ++v) {
            const bool present = (v < channels);
            const float pitchV = present ? inputs[PITCH_INPUT].getVoltage(v) : 0.f;
            // Main gate: matching channel on the gate cable; if gate cable has fewer channels, treat
            // absent as low. (Voice i = pitch[i] + gate[i], per the spec.)
            const bool gateHigh = present
                && inputs[GATE_INPUT].getChannels() > v
                && inputs[GATE_INPUT].getVoltage(v) >= 1.f;
            // Within-legato (step) gate: inner note divisions inside a held legato (MPE_UTILITY_BUILD_SPEC
            // FINAL SHAPE). Additive to the main gate — a rising edge WHILE the main gate is held forces a
            // re-articulation on the SAME member channel, snapping forced re-articulation to a real note
            // division instead of an inferred pitch-threshold point.
            const bool stepGateHigh = present
                && inputs[STEP_GATE_INPUT].getChannels() > v
                && inputs[STEP_GATE_INPUT].getVoltage(v) >= 1.f;

            const bool rising  =  gateHigh && !gatePrev[v];
            const bool falling = !gateHigh &&  gatePrev[v];

            // ── Two-layer expression read. The ACCENT input is the B GATE. Model A
            // (MPE_UTILITY_BUILD_SPEC separation-of-concerns), refined by whether the B gate is PATCHED:
            //   - B gate UNPATCHED: layer B is a free additive layer on A for the WHOLE A (main) gate
            //     (a second always-on expression source — no accent needed).
            //   - B gate PATCHED: layer B applies only during (A gate AND B gate) — the nested AND window,
            //     i.e. B is "the accented sublayer" (same shape as STEP_GATE inside the note gate).
            // Either way B is bounded by A (it is only ever read in the gateHigh branches). bActive is the
            // window B contributes in; bGateHigh additionally drives the unpatched-B "accent" defaults
            // (which only make sense when an accent gate is actually patched + high).
            const bool bGatePatched = inputs[ACCENT_INPUT].isConnected();
            const bool bGateHigh = bGatePatched
                && inputs[ACCENT_INPUT].getChannels() > v
                && inputs[ACCENT_INPUT].getVoltage(v) >= 5.f;
            const bool bActive = gateHigh && (bGatePatched ? bGateHigh : true);

            // X — expressive pitch bend (semitones, rest 0). Added AFTER the note+residual decomposition
            // (bypasses the quantiser — continuous expression pitch, not a scale-degree move). The sum is
            // unbounded here; bend14 clamps to ±bendRange at the wire, and over-range feeds re-articulation.
            const float xA = dotModular::mpe::xSemisFromVolts(readPoly(X_A_INPUT, v));
            // B over bActive: whole A gate if the B gate is unpatched, else only the A∧B accent window.
            const float xB = bActive ? dotModular::mpe::xSemisFromVolts(readPoly(X_B_INPUT, v)) : 0.f;
            const float xTotal = dotModular::mpe::sumAroundRest(xA, xB, 0.f, -1000.f, 1000.f);

            // Y — CC74 timbre (0-127, rest 64). Patched Y_B applies over bActive; the accent-brighten
            // default (Y_B unpatched) needs an actual accent gate high (bGateHigh). Rest 64 otherwise.
            const int yA = dotModular::mpe::yCc74FromVolts(readPoly(Y_A_INPUT, v));
            const int yB = inputs[Y_B_INPUT].isConnected()
                ? (bActive ? dotModular::mpe::yCc74FromVolts(readPoly(Y_B_INPUT, v)) : 64)
                : (bGateHigh ? 64 + yAccentBrighten : 64);
            const int yVal = (int)std::lround(dotModular::mpe::sumAroundRest(
                (float)yA, (float)yB, 64.f, 0.f, 127.f));

            // Z — channel pressure (0-127, rest 0). B over bActive: whole A gate if the B gate is
            // unpatched, else only the A∧B accent window (also silent when Z_B unpatched).
            const int zA = dotModular::mpe::zPressureFromVolts(readPoly(Z_A_INPUT, v));
            const int zB = bActive ? dotModular::mpe::zPressureFromVolts(readPoly(Z_B_INPUT, v)) : 0;
            const int zVal = (int)std::lround(dotModular::mpe::sumAroundRest(
                (float)zA, (float)zB, 0.f, 0.f, 127.f));

            if (rising) {
                // Why Keppel exists — vs stock CV→MIDI. Core CV_MIDI.cpp does the SAME nearest-note round
                // (note = round(pitchV*12 + 60), clamp 0..127) but then DISCARDS the fractional cents: its
                // pitch wheel is a separate global PW input read on channel 0 only, not derived from the
                // pitch CV, so microtonal detail is rounded away and 12-TET is all that reaches the DAW.
                // (The round lives in the MODULE, not dsp::MidiGenerator — that class is transport-only and
                // takes an already-integer note via setNoteGate.) Keppel rounds the same way with noteFor,
                // then KEEPS the remainder (centsOffsetSemis) and encodes it as a PER-VOICE 14-bit bend on
                // this voice's member channel — recovering exactly the information stock CV_MIDI throws out.
                const int   note  = dotModular::mpe::noteFor(pitchV);
                // Bend at note-on = centred residual + X expression (X bypasses the quantiser).
                const float resid = dotModular::mpe::centsOffsetSemis(pitchV);
                const int   pw14  = dotModular::mpe::bend14(resid + xTotal, (float)bendRange);
                // Velocity at note-on (one-shot, latched). Two-layer, Model A, evaluated at the onset:
                // A = VEL CV (patched) or the velNormal base; B = the B layer, applied over bActive
                // (whole note when the B gate is UNPATCHED → a free additive boost; only the A∧B window
                // when the B gate IS patched). The unpatched-B default (the velAccent boost) needs an
                // actual accent gate high (bGateHigh) — no accent gate patched ⇒ plain velNormal.
                int velA, velB;
                if (inputs[VEL_INPUT].isConnected()) {
                    velA = clamp127((int)std::round(inputs[VEL_INPUT].getPolyVoltage(v) / 10.f * 127.f), 1);
                    velB = (bActive && inputs[VEL_B_INPUT].isConnected())
                        ? clamp127((int)std::round(inputs[VEL_B_INPUT].getPolyVoltage(v) / 10.f * 127.f), 0)
                        : 0;
                } else {
                    velA = velNormal;
                    velB = inputs[VEL_B_INPUT].isConnected()
                        ? (bActive ? clamp127((int)std::round(inputs[VEL_B_INPUT].getPolyVoltage(v) / 10.f * 127.f), 0) : 0)
                        : (bGateHigh ? (velAccent - velNormal) : 0);
                }
                const int vel = clamp127(velA + velB, 1);
                // The core allocates the member channel and emits bend → note-on → CC74 → pressure in
                // that order (note opens at pitch with the right timbre/pressure).
                tx.noteOn(v, note, pw14, yVal, zVal, vel, args.frame);
            } else if (falling) {
                // RELEASE VELOCITY: sample the VEL input at the falling edge (how the "key" was released);
                // neutral 64 when VEL is unpatched. A dedicated REL_VEL jack is a possible follow-up (a
                // panel change) — reusing VEL at note-off needs no new I/O and is meaningful when VEL is
                // modulated per note. The core emits 0x8 + this velocity and frees the member channel.
                const int relVel = inputs[VEL_INPUT].isConnected()
                    ? clamp127((int)std::round(inputs[VEL_INPUT].getPolyVoltage(v) / 10.f * 127.f), 0)
                    : 64;
                tx.noteOff(v, relVel, args.frame);
            } else if (gateHigh && tx.active(v) && tx.voices[v].memberCh >= 0) {
                // Total bend = tuning residual (latched note) + X expression. Over-range (past ±bendRange)
                // OR a within-legato step-gate rising edge forces a re-articulation on the SAME member
                // channel — correct pitch / real note boundary at the cost of a retrigger. Re-noting
                // recentres the residual near 0, so the pitch-exceed path does NOT oscillate at the edge.
                const float resid = dotModular::mpe::offsetFromNoteSemis(pitchV, tx.noteOf(v));
                const float totalOffset = resid + xTotal;
                const bool stepRise = stepGateHigh && !stepGatePrev[v];
                const bool exceed = reArticulateOnExceed && std::fabs(totalOffset) > (float)bendRange;
                if (exceed || stepRise) {
                    const int newNote = dotModular::mpe::noteFor(pitchV);
                    const int newBend = dotModular::mpe::bend14(dotModular::mpe::centsOffsetSemis(pitchV) + xTotal, (float)bendRange);
                    // Core: note-off (neutral release) → bend → note-on (reused vel) → re-affirm Y/Z.
                    tx.rearticulate(v, newNote, newBend, yVal, zVal, args.frame);
                } else {
                    // Continuous tracking: the MIDI note stays latched and only bend/Y/Z move, so glides /
                    // timbre / pressure sweeps within range play smoothly with no re-articulation. The core
                    // dedupes each so a held voice doesn't flood the wire at audio rate.
                    const int pw14 = dotModular::mpe::bend14(totalOffset, (float)bendRange);
                    tx.updateContinuous(v, pw14, yVal, zVal, args.frame);
                }
            }

            // Reverse-calc MONITOR: the 1V/oct pitch an ideal MPE receiver reconstructs from what we
            // actually emit (latched note + last bend, at the current range — the last bend already
            // includes the X expression, so the monitor faithfully reflects the emitted pitch). Patch to a
            // scope alongside PITCH in — they overlay to sub-cent when Keppel is correct; any gap (e.g. a
            // clamped slide with re-articulation OFF, or X pushed past the range) is visible.
            if (present) {
                const float mv = (tx.active(v) && tx.voices[v].memberCh >= 0)
                    ? dotModular::mpe::reconstructVolts(tx.noteOf(v), tx.lastBend14(v), (float)bendRange)
                    : 0.f;
                outputs[MONITOR_OUTPUT].setVoltage(mv, v);
            }

            gatePrev[v] = gateHigh;
            stepGatePrev[v] = stepGateHigh;
            anyActive = anyActive || tx.active(v);
        }

        // Carry every message the core queued this block out to the MIDI port.
        drain();
        lights[ACTIVE_LIGHT].setBrightness(anyActive ? 1.f : 0.f);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "midi", midiOut.toJson());
        json_object_set_new(root, "memberCount", json_integer(tx.memberCount));
        json_object_set_new(root, "velNormal", json_integer(velNormal));
        json_object_set_new(root, "velAccent", json_integer(velAccent));
        json_object_set_new(root, "reArticulateOnExceed", json_boolean(reArticulateOnExceed));
        json_object_set_new(root, "fixedAlloc", json_boolean(tx.alloc == dotModular::mpe::MpeTransmitter::Alloc::Fixed));
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* m = json_object_get(root, "midi")) midiOut.fromJson(m);
        if (json_t* mc = json_object_get(root, "memberCount")) {
            int v = (int)json_integer_value(mc);
            tx.memberCount = v < 1 ? 1 : (v > MAX_MEMBERS ? MAX_MEMBERS : v);
        }
        auto readVel = [&](const char* key, int& dst) {
            if (json_t* j = json_object_get(root, key)) {
                int v = (int)json_integer_value(j);
                dst = v < 1 ? 1 : (v > 127 ? 127 : v);
            }
        };
        readVel("velNormal", velNormal);
        readVel("velAccent", velAccent);
        if (json_t* j = json_object_get(root, "reArticulateOnExceed"))
            reArticulateOnExceed = json_boolean_value(j);
        if (json_t* j = json_object_get(root, "fixedAlloc"))
            tx.alloc = json_boolean_value(j) ? dotModular::mpe::MpeTransmitter::Alloc::Fixed
                                             : dotModular::mpe::MpeTransmitter::Alloc::Dynamic;
        midiOut.channel = -1;
        tx.needHandshake = true;   // re-handshake after a patch load
    }
};

struct KeppelWidget : ModuleWidget,
    dotModular::Compose<KeppelWidget, dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;

    KeppelWidget(Keppel* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/Keppel_panel_dark.svg";
        const char* lightPath = "res/panels/Keppel_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<Trimpot>("param_bendrange", KeppelIds::BEND_RANGE_PARAM);
        bindInput<PJ301MPort>("input_pitch",  KeppelIds::PITCH_INPUT);
        bindInput<PJ301MPort>("input_gate",   KeppelIds::GATE_INPUT);
        bindInput<PJ301MPort>("input_accent", KeppelIds::ACCENT_INPUT);
        bindInput<PJ301MPort>("input_vel",    KeppelIds::VEL_INPUT);
        bindLight<SmallLight<GreenLight>>("light_active", KeppelIds::ACTIVE_LIGHT);
        // Two-layer expression jacks + within-legato gate (MPE_UTILITY_BUILD_SPEC Tier 2), bound by name
        // to the generated anchors (panel_src/gen_keppel.py is the single geometry source). Layout groups
        // jacks by dimension as [A][B] pairs (X, Y, Z, velocity) + the step gate. Literal names (not a
        // lambda) so the anchor-vs-bind audit (test/audit_anchor_bind.py) sees every bind 1:1.
        bindInput<PJ301MPort>("input_x_a",       KeppelIds::X_A_INPUT);
        bindInput<PJ301MPort>("input_x_b",       KeppelIds::X_B_INPUT);
        bindInput<PJ301MPort>("input_y_a",       KeppelIds::Y_A_INPUT);
        bindInput<PJ301MPort>("input_y_b",       KeppelIds::Y_B_INPUT);
        bindInput<PJ301MPort>("input_z_a",       KeppelIds::Z_A_INPUT);
        bindInput<PJ301MPort>("input_z_b",       KeppelIds::Z_B_INPUT);
        bindInput<PJ301MPort>("input_vel_b",     KeppelIds::VEL_B_INPUT);
        bindInput<PJ301MPort>("input_step_gate", KeppelIds::STEP_GATE_INPUT);
        // Reverse-calc monitor jack (bound to the output_monitor anchor).
        bindOutput<PJ301MPort>("output_monitor", KeppelIds::MONITOR_OUTPUT);

        // MIDI device panel on the midi_display marker.
        if (auto* s = findNamed("midi_display")) {
            auto* md = createWidget<app::MidiDisplay>(boundsOf(s).pos);
            md->box.size = boundsOf(s).size;
            md->setMidiPort(mod ? &mod->midiOut : nullptr);
            addChild(md);
        }
    }

    void appendContextMenu(Menu* menu) override {
        auto* mod = dynamic_cast<Keppel*>(module);
        if (!mod) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("MPE (lower zone, master ch 1)"));
        // Member-zone size 1..15.
        menu->addChild(createSubmenuItem("Member channels", std::to_string(mod->tx.memberCount),
            [mod](Menu* sub) {
                for (int n : {4, 8, 12, 15}) {
                    sub->addChild(createCheckMenuItem(std::to_string(n) + " voices", "",
                        [mod, n]() { return mod->tx.memberCount == n; },
                        [mod, n]() { mod->panic(); mod->tx.memberCount = n; mod->tx.needHandshake = true; }));
                }
            }));
        // Two-level velocity from the ACCENT input.
        menu->addChild(createSubmenuItem("Normal velocity", std::to_string(mod->velNormal),
            [mod](Menu* sub) {
                for (int n : {40, 64, 80, 100}) {
                    sub->addChild(createCheckMenuItem(std::to_string(n), "",
                        [mod, n]() { return mod->velNormal == n; },
                        [mod, n]() { mod->velNormal = n; }));
                }
            }));
        menu->addChild(createSubmenuItem("Accent velocity", std::to_string(mod->velAccent),
            [mod](Menu* sub) {
                for (int n : {100, 110, 120, 127}) {
                    sub->addChild(createCheckMenuItem(std::to_string(n), "",
                        [mod, n]() { return mod->velAccent == n; },
                        [mod, n]() { mod->velAccent = n; }));
                }
            }));
        menu->addChild(new MenuSeparator);
        // Channel assignment: Fixed = stable voice→channel (best for CV→MPE and round-tripping back);
        // Dynamic = LRU first-free + steal (standard MPE, for a real synth / voices > member channels).
        using Alloc = dotModular::mpe::MpeTransmitter::Alloc;
        menu->addChild(createSubmenuItem("Channel assignment",
            mod->tx.alloc == Alloc::Fixed ? "Fixed (voice=channel)" : "Dynamic (LRU)",
            [mod](Menu* sub) {
                sub->addChild(createCheckMenuItem("Fixed (voice = channel)", "",
                    [mod]() { return mod->tx.alloc == Alloc::Fixed; },
                    [mod]() { mod->panic(); mod->tx.alloc = Alloc::Fixed; }));
                sub->addChild(createCheckMenuItem("Dynamic (LRU + steal)", "",
                    [mod]() { return mod->tx.alloc == Alloc::Dynamic; },
                    [mod]() { mod->panic(); mod->tx.alloc = Alloc::Dynamic; }));
            }));
        menu->addChild(createBoolPtrMenuItem("Re-articulate on big slides", "", &mod->reArticulateOnExceed));
        menu->addChild(createMenuItem("Panic (all notes off)", "", [mod]() { mod->panic(); }));
        menu->addChild(createMenuItem("Re-send MPE config", "", [mod]() { mod->tx.needHandshake = true; }));
    }

    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        // Keppel has no host; theme follows the global setting via the panel's own default (dark).
        // (Kept simple: no Monsoon lookup — this is a standalone utility.)
    }

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        auto f = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!f) return;
        nvgFontFaceId(args.vg, f->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        // ── Every label is positioned from a panel anchor (gen_keppel.py is the SINGLE geometry
        // source); the widget places NOTHING with mm2px. Jack labels sit a fixed dy above their jack
        // anchor — the only two free constants for the jack-label set are JACK_LABEL_DY and the label
        // font size — so moving a jack in the generator moves its label automatically. The title, "bend
        // range" and "→ MPE OUT" sit on their own dedicated anchors (no dy). Each anchor name is passed
        // as a LITERAL to findNamed (not via a parameter) so the anchor-vs-bind audit sees every
        // consumption 1:1 (test/audit_anchor_bind.py).
        auto drawAt = [&](NSVGshape* s, const char* txt, float dy) {
            if (s) nvgText(args.vg, centerOf(s).x, centerOf(s).y + dy, txt, nullptr);
        };
        // Title (larger) on the wordmark anchor.
        nvgFontSize(args.vg, 11.f);
        nvgFillColor(args.vg, nvgRGB(0xf0, 0xf0, 0xf0));
        drawAt(findNamed("wordmark"), "Keppel", 0.f);
        // Everything else: small grey.
        nvgFontSize(args.vg, 6.f);
        nvgFillColor(args.vg, nvgRGB(0x8a, 0x94, 0xa0));
        drawAt(findNamed("label_bendrange"), "bend range", 0.f);
        // Jack labels — each derived from its own jack/control anchor, JACK_LABEL_DY px above the centre.
        constexpr float JACK_LABEL_DY = -15.f;   // jack r ≈ 11px + ~4px gap above the well
        drawAt(findNamed("input_pitch"),     "PITCH",  JACK_LABEL_DY);
        drawAt(findNamed("input_gate"),      "GATE",   JACK_LABEL_DY);
        drawAt(findNamed("input_accent"),    "ACCENT", JACK_LABEL_DY);
        drawAt(findNamed("input_step_gate"), "STEP",   JACK_LABEL_DY);
        drawAt(findNamed("input_x_a"),       "X-A",    JACK_LABEL_DY);
        drawAt(findNamed("input_x_b"),       "X-B",    JACK_LABEL_DY);
        drawAt(findNamed("input_y_a"),       "Y-A",    JACK_LABEL_DY);
        drawAt(findNamed("input_y_b"),       "Y-B",    JACK_LABEL_DY);
        drawAt(findNamed("input_z_a"),       "Z-A",    JACK_LABEL_DY);
        drawAt(findNamed("input_z_b"),       "Z-B",    JACK_LABEL_DY);
        drawAt(findNamed("input_vel"),       "VEL",    JACK_LABEL_DY);
        drawAt(findNamed("input_vel_b"),     "VEL-B",  JACK_LABEL_DY);
        drawAt(findNamed("output_monitor"),  "MON",    JACK_LABEL_DY);
        drawAt(findNamed("label_mpeout"), "→ MPE OUT", 0.f);
    }
};

Model* modelKeppel = createModel<Keppel, KeppelWidget>("Keppel");
