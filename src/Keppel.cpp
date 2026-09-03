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
    enum InputIds { PITCH_INPUT, GATE_INPUT, ACCENT_INPUT, VEL_INPUT, NUM_INPUTS };
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

    // Legato past ±bendRange: a single held note+bend can only CLAMP (wrong pitch) there. Default B
    // (MPE_UTILITY_BUILD_SPEC): re-articulate — re-note on the same member channel so the pitch is
    // always correct, at the cost of a retrigger. Menu-toggleable to the old CLAMP behaviour (smooth
    // but capped) for players who patch the step gates and never exceed the range.
    bool  reArticulateOnExceed = true;

    // Per-voice (poly channel) → member-channel assignment + last state, for edge detect + note-off.
    struct VoiceState {
        bool  active     = false; // gate currently high (a note is sounding)
        int   memberCh   = -1;    // MIDI channel index (1..memberCount) or -1
        int   note       = 60;    // latched MIDI note (fixed for the note's lifetime)
        int   vel        = 80;    // latched note-on velocity (reused when re-articulating past ±range)
        int   lastBend14 = 8192;  // last bend value sent (dedupe held-voice re-sends)
        bool  gatePrev   = false; // previous-block gate for edge detection
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
        configInput(GATE_INPUT,   "Poly gate");
        configInput(ACCENT_INPUT, "Poly accent gate (→ note-on velocity; unpatched = normal)");
        configInput(VEL_INPUT,    "Poly velocity CV (0–10V → 1–127; overrides accent when patched)");
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

        for (int v = 0; v < 16; ++v) {
            const bool present = (v < channels);
            const float pitchV = present ? inputs[PITCH_INPUT].getVoltage(v) : 0.f;
            // Gate: matching channel on the gate cable; if gate cable has fewer channels, treat absent
            // as low. (Voice i = pitch[i] + gate[i], per the spec.)
            const bool gateHigh = present
                && inputs[GATE_INPUT].getChannels() > v
                && inputs[GATE_INPUT].getVoltage(v) >= 1.f;

            VoiceState& vs = voices[v];
            const bool rising  =  gateHigh && !vs.gatePrev;
            const bool falling = !gateHigh &&  vs.gatePrev;

            if (rising) {
                const int note  = dotModular::mpe::noteFor(pitchV);
                const int pw14  = dotModular::mpe::bend14For(pitchV, (float)bendRange);
                const int ch    = allocMember(v, args.frame);
                if (ch >= 0) {
                    // Velocity at note-on (latched — it's a note-on property). Precedence:
                    //   1) VEL CV patched → continuous 0–10V → 1–127 (Rack convention, core CV-MIDI).
                    //   2) else ACCENT gate patched → two-level (velNormal / velAccent).
                    //   3) else → velNormal.
                    int vel;
                    if (inputs[VEL_INPUT].isConnected()) {
                        vel = (int)std::round(inputs[VEL_INPUT].getPolyVoltage(v) / 10.f * 127.f);
                        vel = vel < 1 ? 1 : (vel > 127 ? 127 : vel);
                    } else {
                        const bool accented = inputs[ACCENT_INPUT].getChannels() > v
                                           && inputs[ACCENT_INPUT].getVoltage(v) >= 5.f;
                        vel = accented ? velAccent : velNormal;
                    }
                    // BEND FIRST, THEN note-on (note starts at pitch, not sliding in).
                    sendBend((uint8_t)ch, pw14, args.frame);
                    sendNoteOn((uint8_t)ch, (uint8_t)note, (uint8_t)vel, args.frame);
                    vs.active = true; vs.memberCh = ch; vs.note = note; vs.vel = vel; vs.lastBend14 = pw14;
                }
            } else if (falling) {
                if (vs.memberCh >= 0) {
                    sendNoteOff((uint8_t)vs.memberCh, (uint8_t)vs.note, args.frame);
                    freeMember(vs.memberCh);
                }
                vs.active = false; vs.memberCh = -1;
            } else if (gateHigh && vs.active && vs.memberCh >= 0) {
                const float offset = dotModular::mpe::offsetFromNoteSemis(pitchV, vs.note);
                if (reArticulateOnExceed && std::fabs(offset) > (float)bendRange) {
                    // Legato landmine: the live pitch has drifted past ±bendRange, where a single held
                    // note+bend can only CLAMP (wrong pitch). Re-articulate on the SAME member channel:
                    // note-off the old note, then note-on the new nearest note at a centred bend, reusing
                    // the voice's latched velocity. Correct pitch at the cost of a retrigger
                    // (MPE_UTILITY_BUILD_SPEC default B). Re-noting recentres the offset near 0, so this
                    // does NOT oscillate at the boundary.
                    const int newNote = dotModular::mpe::noteFor(pitchV);
                    const int newBend = dotModular::mpe::bend14For(pitchV, (float)bendRange);
                    sendNoteOff((uint8_t)vs.memberCh, (uint8_t)vs.note, args.frame);
                    sendBend((uint8_t)vs.memberCh, newBend, args.frame);   // bend before note-on
                    sendNoteOn((uint8_t)vs.memberCh, (uint8_t)newNote, (uint8_t)vs.vel, args.frame);
                    vs.note = newNote; vs.lastBend14 = newBend;
                } else {
                    // Continuous bend tracking (option 1): the MIDI note stays latched (vs.note) and only
                    // the per-voice bend moves, so glides/vibrato within ±bendRange play smoothly with no
                    // re-articulation. Beyond the range (re-articulation OFF), bend14 clamps at the
                    // extreme. Re-send only on a change (dedupe) to avoid flooding at audio rate.
                    const int pw14 = dotModular::mpe::bend14FromNote(pitchV, vs.note, (float)bendRange);
                    if (pw14 != vs.lastBend14) {
                        sendBend((uint8_t)vs.memberCh, pw14, args.frame);
                        vs.lastBend14 = pw14;
                    }
                }
            }

            // Reverse-calc MONITOR: the 1V/oct pitch an ideal MPE receiver reconstructs from what we
            // actually emit (latched note + last bend, at the current range). Patch to a scope alongside
            // PITCH in — they overlay to sub-cent when Keppel is correct; any gap (e.g. a clamped slide
            // with re-articulation OFF) is visible. Internal ground truth for the round-trip test.
            if (present) {
                const float mv = (vs.active && vs.memberCh >= 0)
                    ? dotModular::mpe::reconstructVolts(vs.note, vs.lastBend14, (float)bendRange)
                    : 0.f;
                outputs[MONITOR_OUTPUT].setVoltage(mv, v);
            }

            vs.gatePrev = gateHigh;
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
        // Reverse-calc monitor jack. Needs an `output_monitor` marker in the Keppel panel SVGs to render
        // (a small Phase-2 panel task); until it's added, the OUTPUT still exists and drives — it's just
        // not shown on the panel. Guarded so the module compiles/loads either way.
        if (findNamed("output_monitor"))
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
        if (f) {
            nvgFontFaceId(args.vg, f->handle);
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFontSize(args.vg, 11.f);
            nvgFillColor(args.vg, nvgRGB(0xf0, 0xf0, 0xf0));
            nvgText(args.vg, box.size.x * 0.5f, mm2px(12.0f), "Keppel", nullptr);
            nvgFontSize(args.vg, 6.f);
            nvgFillColor(args.vg, nvgRGB(0x8a, 0x94, 0xa0));
            nvgText(args.vg, box.size.x * 0.5f, mm2px(24.0f), "bend range", nullptr);
            // 2×2 input grid labels (columns at CX∓8.5mm; jacks at rows 96 / 116mm).
            const float colL = mm2px(20.32f - 8.5f), colR = mm2px(20.32f + 8.5f);
            nvgText(args.vg, colL, mm2px(90.0f),  "PITCH",  nullptr);
            nvgText(args.vg, colR, mm2px(90.0f),  "GATE",   nullptr);
            nvgText(args.vg, colL, mm2px(110.0f), "ACCENT", nullptr);
            nvgText(args.vg, colR, mm2px(110.0f), "VEL",    nullptr);
            nvgText(args.vg, box.size.x * 0.5f, mm2px(125.5f), "→ MPE OUT", nullptr);
        }
    }
};

Model* modelKeppel = createModel<Keppel, KeppelWidget>("Keppel");
