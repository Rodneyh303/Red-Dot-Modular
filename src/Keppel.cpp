#include <rack.hpp>
#include <cmath>
#include <vector>
#include "Monsoon.hpp"                    // pluginInstance
#include "ui/SvgPanelKit.hpp"
#include "dsp/MpeMath.hpp"

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

    // MPE Lower Zone: master = MIDI channel 1 (index 0); members = channels 2..(1+memberCount)
    // (indices 1..memberCount). Default 15 members (the full lower zone).
    static constexpr int MAX_MEMBERS = 15;
    int   memberCount = MAX_MEMBERS;

    // Two-level velocity from the ACCENT input (dot.modular's accent is a 10V/0V gate). Sampled per
    // voice at note-on: accent-gate high → velAccent, else velNormal. Unpatched ACCENT → every note
    // uses velNormal (fully back-compatible). Both menu-adjustable (receivers vary in velocity curve).
    int   velNormal = 80;
    int   velAccent = 127;
    // Opinionated default for the Y (CC74) brighten on accent when Y_B is unpatched (MPE_UTILITY_BUILD_SPEC
    // "Two-layer": defaults apply ONLY when a B jack is unpatched — velocity boost + Y brighten on accent,
    // X and Z silent). +32 CC74 ≈ half-range toward open/bright.
    int   yAccentBrighten = 32;

    // Legato past ±bendRange: a single held note+bend can only CLAMP (wrong pitch) there. Default B
    // (MPE_UTILITY_BUILD_SPEC): re-articulate — re-note on the same member channel so the pitch is
    // always correct, at the cost of a retrigger. Menu-toggleable to the old CLAMP behaviour (smooth
    // but capped) for players who patch the step gates and never exceed the range.
    bool  reArticulateOnExceed = true;

    // Per-voice (poly channel) → member-channel assignment + last state, for edge detect + note-off.
    struct VoiceState {
        bool  active       = false; // gate currently high (a note is sounding)
        int   memberCh     = -1;    // MIDI channel index (1..memberCount) or -1
        int   note         = 60;    // latched MIDI note (fixed for the note's lifetime)
        int   vel          = 80;    // latched note-on velocity (reused when re-articulating past ±range)
        int   lastBend14   = 8192;  // last bend value sent (dedupe held-voice re-sends)
        int   lastY74      = 64;    // last CC74 (timbre) sent — dedupe (carried across re-articulation/LRU)
        int   lastZ7       = 0;     // last channel pressure sent — dedupe (carried across re-articulation/LRU)
        bool  gatePrev     = false; // previous-block main gate for edge detection
        bool  stepGatePrev = false; // previous-block within-legato (step) gate for inner-boundary detect
    };
    VoiceState voices[16];

    // Member-channel occupancy (index 1..memberCount used; 0 = master, unused for notes). Value = voice
    // index owning it, or -1 free. lruOrder tracks age for stealing (front = oldest).
    int  memberOwner[16];
    std::vector<int> lruOrder;    // member-channel indices, oldest first

    float lastBendRange = -1.f;   // triggers the MPE config handshake on change
    bool  needHandshake = true;   // send on init / device change / range change

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
        for (int i = 0; i < 16; ++i) memberOwner[i] = -1;
        midiOut.channel = -1;     // we set each message's channel ourselves (per-voice MPE)
    }

    // ── raw MIDI send helpers (build a Message, set its channel, send) ────────────────────────────
    void send3(uint8_t status, uint8_t ch, uint8_t d1, uint8_t d2, int64_t frame) {
        midi::Message m;                       // 3 bytes by default
        m.setStatus(status);
        m.setChannel(ch);
        m.setNote(d1);
        m.setValue(d2);
        m.setFrame(frame);
        midiOut.sendMessage(m);
    }
    void sendCC(uint8_t ch, uint8_t cc, uint8_t val, int64_t frame)  { send3(0xB, ch, cc, val, frame); }
    void sendNoteOn(uint8_t ch, uint8_t note, uint8_t vel, int64_t f){ send3(0x9, ch, note, vel, f); }
    void sendNoteOff(uint8_t ch, uint8_t note, int64_t f)           { send3(0x8, ch, note, 0, f); }
    void sendBend(uint8_t ch, int pw14, int64_t frame) {
        send3(0xE, ch, (uint8_t)(pw14 & 0x7f), (uint8_t)((pw14 >> 7) & 0x7f), frame);
    }
    // Channel Pressure (MPE Z dimension): a 2-byte message (0xDn, pressure). Built explicitly with
    // setSize(2) so the third byte isn't sent — most receivers tolerate a stray 0, but MPE receivers
    // are picky and we want the wire bytes exact.
    void sendPressure(uint8_t ch, uint8_t val, int64_t frame) {
        midi::Message m;
        m.setSize(2);
        m.setStatus(0xD);
        m.setChannel(ch);
        m.bytes[1] = val & 0x7f;
        m.setFrame(frame);
        midiOut.sendMessage(m);
    }

    // ── MPE Configuration handshake: set the lower-zone member count + per-note bend range. Sent on
    // init / device change / bend-range change (the make-or-break RPN sequence). ─────────────────────
    void sendMpeConfig(int bendRangeSemis, int64_t frame) {
        // 1) MPE Configuration Message (RPN 6) on the MASTER channel (index 0): member count.
        sendCC(0, 101, 0x00, frame);   // RPN MSB
        sendCC(0, 100, 0x06, frame);   // RPN LSB = 6 (MCM)
        sendCC(0,   6, (uint8_t)memberCount, frame);   // Data Entry MSB = member channel count
        sendCC(0, 101, 0x7f, frame);   // RPN Null (close MCM before opening the next RPN)
        sendCC(0, 100, 0x7f, frame);
        // 2) Pitch-bend sensitivity (RPN 0) = bendRangeSemis, on master + every member channel.
        //    Master + members both carry the range: members use it for the per-note microtonal bend;
        //    master range is harmless (we never send master-channel bends). Matches moDllz MIDIpolyMPE.
        for (int ch = 0; ch <= memberCount; ++ch) {
            sendCC(ch, 101, 0x00, frame);            // RPN MSB
            sendCC(ch, 100, 0x00, frame);            // RPN LSB = 0 (pitch-bend sensitivity)
            sendCC(ch,   6, (uint8_t)bendRangeSemis, frame);  // Data Entry MSB = semitones
            sendCC(ch,  38, 0x00, frame);            // Data Entry LSB = cents (0)
            sendCC(ch, 101, 0x7f, frame);            // RPN Null (close)
            sendCC(ch, 100, 0x7f, frame);
        }
    }

    // Send note-off for everything sounding (device change / teardown) — no stuck notes.
    void allNotesOff(int64_t frame) {
        for (int v = 0; v < 16; ++v) {
            if (voices[v].active && voices[v].memberCh >= 0)
                sendNoteOff((uint8_t)voices[v].memberCh, (uint8_t)voices[v].note, frame);
            voices[v].active = false;
            voices[v].memberCh = -1;
        }
        for (int i = 0; i < 16; ++i) memberOwner[i] = -1;
        lruOrder.clear();
    }

    // Allocate a free member channel (1..memberCount) for voice v; steal the oldest if none free.
    int allocMember(int v, int64_t frame) {
        for (int ch = 1; ch <= memberCount; ++ch) {
            if (memberOwner[ch] < 0) {
                memberOwner[ch] = v; lruOrder.push_back(ch); return ch;
            }
        }
        // None free → steal the oldest (front of LRU): note-off its owner first.
        if (!lruOrder.empty()) {
            int ch = lruOrder.front(); lruOrder.erase(lruOrder.begin());
            int prevOwner = memberOwner[ch];
            if (prevOwner >= 0 && voices[prevOwner].active) {
                sendNoteOff((uint8_t)ch, (uint8_t)voices[prevOwner].note, frame);
                voices[prevOwner].active = false;
                voices[prevOwner].memberCh = -1;
            }
            memberOwner[ch] = v; lruOrder.push_back(ch); return ch;
        }
        return -1;
    }
    void freeMember(int ch) {
        if (ch < 0) return;
        memberOwner[ch] = -1;
        for (auto it = lruOrder.begin(); it != lruOrder.end(); ++it)
            if (*it == ch) { lruOrder.erase(it); break; }
    }

    void onReset() override {
        allNotesOff(-1);
        needHandshake = true;
        lastBendRange = -1.f;
    }

    void process(const ProcessArgs& args) override {
        using namespace KeppelIds;
        // MPE requires per-message channels: keep the Port from force-overwriting them. The on-panel
        // MidiDisplay channel row can set this live, so re-assert every block (it self-corrects to
        // "All channels"). Driver/device selection on the display is unaffected.
        midiOut.channel = -1;
        const int bendRange = (int)std::round(params[BEND_RANGE_PARAM].getValue());

        // (Re)send the MPE config on init or when the bend range changes.
        if (needHandshake || (float)bendRange != lastBendRange) {
            sendMpeConfig(bendRange, args.frame);
            lastBendRange = (float)bendRange;
            needHandshake = false;
        }

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

            VoiceState& vs = voices[v];
            const bool rising  =  gateHigh && !vs.gatePrev;
            const bool falling = !gateHigh &&  vs.gatePrev;

            // ── Two-layer expression read (per dimension: A main-gated base + B accent-gated additive).
            // The accent window opens ONLY while the accent gate is high AND the note is live (main gate
            // held). Computed every block for use in the rising + hold branches.
            const bool accentLive = gateHigh
                && inputs[ACCENT_INPUT].getChannels() > v
                && inputs[ACCENT_INPUT].getVoltage(v) >= 5.f;

            // X — expressive pitch bend (semitones, rest 0). Added AFTER the note+residual decomposition
            // (bypasses the quantiser — continuous expression pitch, not a scale-degree move). The sum is
            // unbounded here; bend14 clamps to ±bendRange at the wire, and over-range feeds re-articulation.
            const float xA = dotModular::mpe::xSemisFromVolts(readPoly(X_A_INPUT, v));
            const float xB = dotModular::mpe::xSemisFromVolts(readPoly(X_B_INPUT, v)); // 0 (silent) when unpatched
            const float xTotal = dotModular::mpe::sumAroundRest(xA, xB, 0.f, -1000.f, 1000.f);

            // Y — CC74 timbre (0-127, rest 64). B default: accent brighten when Y_B unpatched.
            const int yA = dotModular::mpe::yCc74FromVolts(readPoly(Y_A_INPUT, v));
            const int yB = inputs[Y_B_INPUT].isConnected()
                ? dotModular::mpe::yCc74FromVolts(readPoly(Y_B_INPUT, v))
                : (accentLive ? 64 + yAccentBrighten : 64);
            const int yVal = (int)std::lround(dotModular::mpe::sumAroundRest(
                (float)yA, (float)yB, 64.f, 0.f, 127.f));

            // Z — channel pressure (0-127, rest 0). B silent unless patched.
            const int zA = dotModular::mpe::zPressureFromVolts(readPoly(Z_A_INPUT, v));
            const int zB = dotModular::mpe::zPressureFromVolts(readPoly(Z_B_INPUT, v)); // 0 (silent) when unpatched
            const int zVal = (int)std::lround(dotModular::mpe::sumAroundRest(
                (float)zA, (float)zB, 0.f, 0.f, 127.f));

            if (rising) {
                const int note  = dotModular::mpe::noteFor(pitchV);
                // Bend at note-on = centred residual + X expression (X bypasses the quantiser).
                const float resid = dotModular::mpe::centsOffsetSemis(pitchV);
                const int    pw14 = dotModular::mpe::bend14(resid + xTotal, (float)bendRange);
                const int    ch   = allocMember(v, args.frame);
                if (ch >= 0) {
                    // Velocity at note-on (one-shot, latched). Two-layer: A = VEL CV (patched) or the
                    // two-level fallback (velNormal / velAccent); B = VEL_B CV (patched, additive) or the
                    // default accent boost. Back-compat: VEL patched overrides accent entirely (velB=0
                    // default), matching the original "overrides accent when patched". With NEITHER VEL nor
                    // VEL_B patched, accent → velAccent, else velNormal (exactly the original behaviour).
                    int velA, velB;
                    if (inputs[VEL_INPUT].isConnected()) {
                        velA = clamp127((int)std::round(inputs[VEL_INPUT].getPolyVoltage(v) / 10.f * 127.f), 1);
                        velB = inputs[VEL_B_INPUT].isConnected()
                            ? clamp127((int)std::round(inputs[VEL_B_INPUT].getPolyVoltage(v) / 10.f * 127.f), 0)
                            : 0;   // no accent default when VEL is patched (user took explicit control)
                    } else {
                        velA = velNormal;
                        velB = inputs[VEL_B_INPUT].isConnected()
                            ? clamp127((int)std::round(inputs[VEL_B_INPUT].getPolyVoltage(v) / 10.f * 127.f), 0)
                            : (accentLive ? (velAccent - velNormal) : 0);
                    }
                    const int vel = clamp127(velA + velB, 1);
                    // BEND FIRST, THEN note-on (note starts at pitch, not sliding in), THEN initial Y/Z so
                    // the receiver opens the note with the right timbre + pressure (MPE_UTILITY_BUILD_SPEC).
                    sendBend((uint8_t)ch, pw14, args.frame);
                    sendNoteOn((uint8_t)ch, (uint8_t)note, (uint8_t)vel, args.frame);
                    sendCC((uint8_t)ch, 74, (uint8_t)yVal, args.frame);       // initial Y (CC74)
                    sendPressure((uint8_t)ch, (uint8_t)zVal, args.frame);     // initial Z (channel pressure)
                    vs.active = true; vs.memberCh = ch; vs.note = note; vs.vel = vel;
                    vs.lastBend14 = pw14; vs.lastY74 = yVal; vs.lastZ7 = zVal;
                }
            } else if (falling) {
                if (vs.memberCh >= 0) {
                    sendNoteOff((uint8_t)vs.memberCh, (uint8_t)vs.note, args.frame);
                    freeMember(vs.memberCh);
                }
                vs.active = false; vs.memberCh = -1;
            } else if (gateHigh && vs.active && vs.memberCh >= 0) {
                // Total bend = tuning residual (vs.note latched) + X expression. Over-range (past
                // ±bendRange) OR a within-legato step-gate rising edge forces a re-articulation on the SAME
                // member channel — correct pitch / real note boundary at the cost of a retrigger. Re-noting
                // recentres the residual near 0, so the pitch-exceed path does NOT oscillate at the edge.
                const float resid = dotModular::mpe::offsetFromNoteSemis(pitchV, vs.note);
                const float totalOffset = resid + xTotal;
                const bool stepRise = stepGateHigh && !vs.stepGatePrev;
                const bool exceed = reArticulateOnExceed && std::fabs(totalOffset) > (float)bendRange;
                if (exceed || stepRise) {
                    const int newNote = dotModular::mpe::noteFor(pitchV);
                    const float newResid = dotModular::mpe::centsOffsetSemis(pitchV);
                    const int newBend = dotModular::mpe::bend14(newResid + xTotal, (float)bendRange);
                    sendNoteOff((uint8_t)vs.memberCh, (uint8_t)vs.note, args.frame);
                    sendBend((uint8_t)vs.memberCh, newBend, args.frame);      // bend before note-on
                    sendNoteOn((uint8_t)vs.memberCh, (uint8_t)newNote, (uint8_t)vs.vel, args.frame);
                    // Re-affirm Y/Z at the new note-on (current values — no discontinuity; carried across).
                    sendCC((uint8_t)vs.memberCh, 74, (uint8_t)yVal, args.frame);
                    sendPressure((uint8_t)vs.memberCh, (uint8_t)zVal, args.frame);
                    vs.note = newNote; vs.lastBend14 = newBend; vs.lastY74 = yVal; vs.lastZ7 = zVal;
                } else {
                    // Continuous tracking: the MIDI note stays latched and only bend/Y/Z move, so glides /
                    // timbre / pressure sweeps within range play smoothly with no re-articulation. Re-send
                    // each only on a change (dedupe, like lastBend14) to avoid flooding at audio rate.
                    const int pw14 = dotModular::mpe::bend14(totalOffset, (float)bendRange);
                    if (pw14 != vs.lastBend14) {
                        sendBend((uint8_t)vs.memberCh, pw14, args.frame);
                        vs.lastBend14 = pw14;
                    }
                    if (yVal != vs.lastY74) {
                        sendCC((uint8_t)vs.memberCh, 74, (uint8_t)yVal, args.frame);
                        vs.lastY74 = yVal;
                    }
                    if (zVal != vs.lastZ7) {
                        sendPressure((uint8_t)vs.memberCh, (uint8_t)zVal, args.frame);
                        vs.lastZ7 = zVal;
                    }
                }
            }

            // Reverse-calc MONITOR: the 1V/oct pitch an ideal MPE receiver reconstructs from what we
            // actually emit (latched note + last bend, at the current range — the last bend already
            // includes the X expression, so the monitor faithfully reflects the emitted pitch). Patch to a
            // scope alongside PITCH in — they overlay to sub-cent when Keppel is correct; any gap (e.g. a
            // clamped slide with re-articulation OFF, or X pushed past the range) is visible.
            if (present) {
                const float mv = (vs.active && vs.memberCh >= 0)
                    ? dotModular::mpe::reconstructVolts(vs.note, vs.lastBend14, (float)bendRange)
                    : 0.f;
                outputs[MONITOR_OUTPUT].setVoltage(mv, v);
            }

            vs.gatePrev = gateHigh;
            vs.stepGatePrev = stepGateHigh;
            anyActive = anyActive || vs.active;
        }

        lights[ACTIVE_LIGHT].setBrightness(anyActive ? 1.f : 0.f);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "midi", midiOut.toJson());
        json_object_set_new(root, "memberCount", json_integer(memberCount));
        json_object_set_new(root, "velNormal", json_integer(velNormal));
        json_object_set_new(root, "velAccent", json_integer(velAccent));
        json_object_set_new(root, "reArticulateOnExceed", json_boolean(reArticulateOnExceed));
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* m = json_object_get(root, "midi")) midiOut.fromJson(m);
        if (json_t* mc = json_object_get(root, "memberCount")) {
            int v = (int)json_integer_value(mc);
            memberCount = v < 1 ? 1 : (v > MAX_MEMBERS ? MAX_MEMBERS : v);
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
        midiOut.channel = -1;
        needHandshake = true;   // re-handshake after a patch load
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
        menu->addChild(createSubmenuItem("Member channels", std::to_string(mod->memberCount),
            [mod](Menu* sub) {
                for (int n : {4, 8, 12, 15}) {
                    sub->addChild(createCheckMenuItem(std::to_string(n) + " voices", "",
                        [mod, n]() { return mod->memberCount == n; },
                        [mod, n]() { mod->allNotesOff(-1); mod->memberCount = n; mod->needHandshake = true; }));
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
        menu->addChild(createBoolPtrMenuItem("Re-articulate on big slides", "", &mod->reArticulateOnExceed));
        menu->addChild(createMenuItem("Panic (all notes off)", "", [mod]() { mod->allNotesOff(-1); }));
        menu->addChild(createMenuItem("Re-send MPE config", "", [mod]() { mod->needHandshake = true; }));
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
