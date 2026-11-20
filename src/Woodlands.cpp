// Woodlands — MPE MIDI IN → poly CV (the mirror of Keppel).  [WORKING NAME — see gen_woodlands.py]
//
// Receives MPE (lower zone) and emits per-voice: true (microtonal) pitch V/OCT with the member bend
// folded in at the set range, GATE, VEL, R-VEL (release velocity), Y (CC74 timbre), Z (channel pressure),
// and a standalone X (raw bend, semitones). Closes the stock VCV MIDI-CV MPE-in gaps (no native CC74 Y,
// split V/OCT+bend, no release velocity).
//
// Thin FACE over the shared, Rack-agnostic, unit-tested core: dotModular::mpe::MpeReceiver does all the
// state; this module pops the MIDI input queue into it and reads the per-voice accessors out to poly
// ports. Shares MpeMath with Keppel, so a Keppel→Woodlands loop is identity (test_MpeRoundTrip).
//
// NOTE: this is the Rack face — it cannot be compiled in the headless test harness (needs the Rack SDK).
// The receiver logic it delegates to IS unit-tested; the face needs a Rack build + a visual feel-test.

#include <rack.hpp>
#include "Monsoon.hpp"                    // pluginInstance
#include "ui/SvgPanelKit.hpp"             // Compose / ShapeQuery / Bind / Reload mixins
#include "dsp/MpeReceiver.hpp"

extern Model* modelWoodlands;

namespace WoodlandsIds {
    enum ParamIds  { BEND_RANGE_PARAM, NUM_PARAMS };
    enum InputIds  { NUM_INPUTS };                         // MIDI arrives via the InputQueue, not a port
    enum OutputIds {
        PITCH_OUTPUT,   // V/OCT with member bend folded in (true microtonal pitch)
        GATE_OUTPUT,
        VEL_OUTPUT,
        RVEL_OUTPUT,    // release velocity
        Y_OUTPUT,       // CC74 timbre → bipolar around 0V
        Z_OUTPUT,       // channel pressure → 0..10V
        X_OUTPUT,       // standalone raw bend, in semitones (1V = 1 semitone)
        NUM_OUTPUTS
    };
    enum LightIds  { ACTIVE_LIGHT, NUM_LIGHTS };
}

struct Woodlands : Module {
    midi::InputQueue midiInput;
    dotModular::mpe::MpeReceiver rx;

    Woodlands() {
        using namespace WoodlandsIds;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(BEND_RANGE_PARAM, 1.f, 48.f, 48.f, "Pitch-bend range", " semitones");
        configOutput(PITCH_OUTPUT, "Pitch (V/OCT, bend folded in)");
        configOutput(GATE_OUTPUT,  "Gate");
        configOutput(VEL_OUTPUT,   "Velocity");
        configOutput(RVEL_OUTPUT,  "Release velocity");
        configOutput(Y_OUTPUT,     "Y / timbre (CC74)");
        configOutput(Z_OUTPUT,     "Z / pressure");
        configOutput(X_OUTPUT,     "X / bend (semitones)");
    }

    void onReset() override { rx.reset(); }

    void process(const ProcessArgs& args) override {
        using namespace WoodlandsIds;
        rx.bendRangeSemis = params[BEND_RANGE_PARAM].getValue();

        // Drain the MIDI input queue into the receiver. getStatus() returns the high nibble and
        // getNote()/getValue() the two data bytes — exactly MpeReceiver::handleMessage's convention.
        midi::Message msg;
        while (midiInput.tryPop(&msg, args.frame)) {
            rx.handleMessage(msg.getStatus(), msg.getChannel(), msg.getNote(), msg.getValue());
        }

        const int n = rx.maxVoices();
        for (int id : {PITCH_OUTPUT, GATE_OUTPUT, VEL_OUTPUT, RVEL_OUTPUT, Y_OUTPUT, Z_OUTPUT, X_OUTPUT})
            outputs[id].setChannels(n);

        bool anyActive = false;
        for (int v = 0; v < n; ++v) {
            outputs[PITCH_OUTPUT].setVoltage(rx.pitchVolts(v),  v);
            outputs[GATE_OUTPUT].setVoltage(rx.gateVolts(v),    v);
            outputs[VEL_OUTPUT].setVoltage(rx.velVolts(v),      v);
            outputs[RVEL_OUTPUT].setVoltage(rx.relVelVolts(v),  v);
            outputs[Y_OUTPUT].setVoltage(rx.yVolts(v),          v);
            outputs[Z_OUTPUT].setVoltage(rx.zVolts(v),          v);
            outputs[X_OUTPUT].setVoltage(rx.xBendSemis(v),      v);
            anyActive = anyActive || rx.active(v);
        }
        lights[ACTIVE_LIGHT].setBrightness(anyActive ? 1.f : 0.f);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "midi", midiInput.toJson());
        json_object_set_new(root, "memberCount", json_integer(rx.memberCount));
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* m = json_object_get(root, "midi")) midiInput.fromJson(m);
        if (json_t* mc = json_object_get(root, "memberCount")) {
            int v = (int)json_integer_value(mc);
            rx.memberCount = v < 1 ? 1 : (v > dotModular::mpe::MpeReceiver::MAX_MEMBERS
                                              ? dotModular::mpe::MpeReceiver::MAX_MEMBERS : v);
        }
    }
};

struct WoodlandsWidget : ModuleWidget,
    dotModular::Compose<WoodlandsWidget, dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    WoodlandsWidget(Woodlands* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/Woodlands_panel_dark.svg";
        const char* lightPath = "res/panels/Woodlands_panel_light.svg";
        (void)lightPath;
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        bindParam<Trimpot>("param_bendrange", WoodlandsIds::BEND_RANGE_PARAM);
        bindLight<SmallLight<GreenLight>>("light_active", WoodlandsIds::ACTIVE_LIGHT);
        // Poly CV outputs, bound by name to the generated anchors (gen_woodlands.py is the single geometry
        // source). Literal names so the anchor-vs-bind audit (test/audit_anchor_bind.py) sees each 1:1.
        bindOutput<PJ301MPort>("output_pitch", WoodlandsIds::PITCH_OUTPUT);
        bindOutput<PJ301MPort>("output_gate",  WoodlandsIds::GATE_OUTPUT);
        bindOutput<PJ301MPort>("output_vel",   WoodlandsIds::VEL_OUTPUT);
        bindOutput<PJ301MPort>("output_rvel",  WoodlandsIds::RVEL_OUTPUT);
        bindOutput<PJ301MPort>("output_y",     WoodlandsIds::Y_OUTPUT);
        bindOutput<PJ301MPort>("output_z",     WoodlandsIds::Z_OUTPUT);
        bindOutput<PJ301MPort>("output_x",     WoodlandsIds::X_OUTPUT);

        // MIDI input device panel on the midi_display marker.
        if (auto* s = findNamed("midi_display")) {
            auto* md = createWidget<app::MidiDisplay>(boundsOf(s).pos);
            md->box.size = boundsOf(s).size;
            md->setMidiPort(mod ? &mod->midiInput : nullptr);
            addChild(md);
        }
    }

    void appendContextMenu(Menu* menu) override {
        auto* mod = dynamic_cast<Woodlands*>(module);
        if (!mod) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("MPE (lower zone, master ch 1)"));
        menu->addChild(createSubmenuItem("Member channels", std::to_string(mod->rx.memberCount),
            [mod](Menu* sub) {
                for (int n : {4, 8, 12, 15}) {
                    sub->addChild(createCheckMenuItem(std::to_string(n) + " voices", "",
                        [mod, n]() { return mod->rx.memberCount == n; },
                        [mod, n]() { mod->rx.memberCount = n; }));
                }
            }));
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Set bend range to match your sender (MPE default 48)"));
    }

    void step() override {
        ModuleWidget::step();
        kitStep();
    }

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        auto f = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!f) return;
        nvgFontFaceId(args.vg, f->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        auto drawAt = [&](NSVGshape* s, const char* txt, float dy) {
            if (s) nvgText(args.vg, centerOf(s).x, centerOf(s).y + dy, txt, nullptr);
        };
        nvgFontSize(args.vg, 11.f);
        nvgFillColor(args.vg, nvgRGB(0xf0, 0xf0, 0xf0));
        drawAt(findNamed("wordmark"), "Woodlands", 0.f);
        nvgFontSize(args.vg, 6.f);
        nvgFillColor(args.vg, nvgRGB(0x8a, 0x94, 0xa0));
        drawAt(findNamed("label_bendrange"), "bend range", 0.f);
        constexpr float JACK_LABEL_DY = -15.f;
        drawAt(findNamed("output_pitch"), "PITCH", JACK_LABEL_DY);
        drawAt(findNamed("output_gate"),  "GATE",  JACK_LABEL_DY);
        drawAt(findNamed("output_vel"),   "VEL",   JACK_LABEL_DY);
        drawAt(findNamed("output_rvel"),  "R-VEL", JACK_LABEL_DY);
        drawAt(findNamed("output_y"),     "Y",     JACK_LABEL_DY);
        drawAt(findNamed("output_z"),     "Z",     JACK_LABEL_DY);
        drawAt(findNamed("output_x"),     "X",     JACK_LABEL_DY);
        drawAt(findNamed("label_mpein"), "MPE IN →", 0.f);
    }
};

Model* modelWoodlands = createModel<Woodlands, WoodlandsWidget>("Woodlands");
