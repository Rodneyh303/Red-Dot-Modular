#include <rack.hpp>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include "MicroTuning.hpp"
#include "Monsoon.hpp"
#include "MonsoonInterchangeExpander.hpp"   // 3C-ii: read paired Interchange CV into weight[]
#include "MonsoonShophouseMicro.hpp"        // Shophouse Micro (Model Q): scene-front FOLD into weight/cents
#include "ui/IntertropicalPairing.hpp"      // redDot::assignPairIdT / findPairHubEitherSide (templates)
#include "ui/VisualExpanderHelpers.hpp"   // redDot::findMonsoonEitherSide
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/WrappingMenuLabel.hpp"    // redDot::makeWrappingMenuLabel — bounds long .scl descriptions
#include "tuning/ScalaFile.hpp"
#include <osdialog.h>
#include "tuning/TuningPreset.hpp"

using namespace rack;
using namespace microTuning;

// ── Module process(): root cents-lock, then (when claimed) publish cents[]+weight[]+maskAuthored
// into the shared TuningTable so Monsoon reads its scale mask from the Micro (Model A delegation).
// Control-rate-cheap; no alloc/IO. Degree-count-generic (12 for Colonnades, 24 for Duo).
void MicroTuningModule::process(const ProcessArgs&) {
    const int n = nDegrees;
    params[centsParam(n, 0)].setValue(0.f);    // root cents locked at 0 (Scalar rule)

    // Pairing HUB: self-assign a rack-wide pairId ONCE (one-shot, like Intertropical/CA — never in a
    // locked context; process() is safe). Interchanges bind to this by pairId.
    if (!pairChecked) { pairChecked = true; if (pairId <= 0) pairId = redDot::assignPairIdT<MicroTuningModule>(this); }

    Monsoon* mon = redDot::findMonsoonEitherSide(this);
    if (!mon) return;                          // standalone: publish nothing (ConnectMark greys)
    if (!mon->claimAsTuningSource(this)) return;   // not the claimant: don't write (loser greys)

    dotModular::TuningTable& tt = mon->getTuningTable();
    // SHOPHOUSE MICRO SCENE (Rodney's model): an active front DRIVES the cents KNOBS + enabledState + the
    // TUNING SIZE (tuningN) directly — so the moving knobs, the LED readout, the dimmed faders AND the
    // greyed-beyond-N faders ALL follow the modulation with no separate display path. On the scene's first
    // active block we CACHE the authored cents/enabled/tuningN (baseCents_/baseEnabled_/baseTuningN_); on
    // DETACH (or no active front) we RESTORE that cache. WEIGHT faders are never touched (the .dmtune has
    // no loudness). A front carries its OWN n (1..capacity, ROUND 10 full model) — switching to a 17-note
    // front makes the host a 17-note tuning. boundShophouseMicro_ is discovered control-rate below.
    auto* scene = dynamic_cast<MonsoonShophouseMicro*>(boundShophouseMicro_);
    // scene->list.degrees() is the Shophouse Micro's CAPACITY/mode (12/24) — must match THIS Micro's
    // capacity. The front's own n (<= capacity) then sizes the tuning.
    const bool sceneActive = scene && scene->hasActiveFront() && scene->list.degrees() == nDegrees;
    if (sceneActive) {
        if (!sceneCacheValid_) {                       // entering a scene → snapshot authored base ONCE
            for (int i = 0; i < nDegrees; ++i) {
                baseCents_[i]   = params[centsParam(nDegrees, i)].getValue();
                baseEnabled_[i] = enabledState[i];
            }
            baseTuningN_ = tuningN;
            sceneCacheValid_ = true;
        }
        const TuningSlot& f = scene->activeFront();    // drive knobs + mask + tuning size from the front
        const int fn = f.n < 1 ? 1 : (f.n > nDegrees ? nDegrees : f.n);
        tuningN = fn;
        for (int i = 0; i < nDegrees; ++i) {
            params[centsParam(nDegrees, i)].setValue(i == 0 ? 0.f : (i < fn ? f.cents[i] : baseCents_[i]));
            enabledState[i] = (i < fn) ? f.enabled[i] : baseEnabled_[i];   // beyond front-n: keep base (greyed)
        }
    } else if (sceneCacheValid_) {                     // scene gone → restore the authored base ONCE
        for (int i = 0; i < nDegrees; ++i) {
            params[centsParam(nDegrees, i)].setValue(baseCents_[i]);
            enabledState[i] = baseEnabled_[i];
        }
        tuningN = baseTuningN_;
        sceneCacheValid_ = false;
    }

    // ENGINE tuning size (THE ROUND-10 caveat): tt.N tracks the TUNING SIZE (liveN), NOT the module
    // capacity — so the engine never generates/quantizes a greyed degree (>= liveN). Everything else
    // publishes the full capacity array (harmless; the engine reads only < tt.N).
    tt.N = liveN();

    // PUBLISH the authored/driven state into the shared table: cents[] from the (now possibly
    // scene-driven) knobs, enabled[] from enabledState[], weight[] from the faders (pure loudness).
    for (int i = 0; i < n; ++i) {
        tt.cents[i]   = params[centsParam(n, i)].getValue();
        tt.weight[i]  = params[weightParam(i)].getValue();
        tt.enabled[i] = enabledState[i];
    }
    // CONSERVATION toggle lives on the bound Shophouse Micro and governs the whole rig's mask, whether
    // that mask came from an ACTIVE FRONT or the Colonnades' OWN enable band — NOT only when a front is
    // active (matches original Shophouse: the toggle governs the effective scale, always). `scene` is the
    // bound module (may have no active front); enforce() reads its switch. When NO Shophouse Micro is
    // bound there is no conservation control → the Colonnades mask stays hard (byte-identical).
    const bool sceneBound   = (scene != nullptr);
    const bool sceneGuide   = sceneBound && !scene->enforce();   // Guide: mask is a visual guide only
    // 3C-ii: fold in CV from any Interchange(s) bound to THIS Micro (single-writer: we own weight[], so
    // we add the mod into our published copy — the stored fader params are untouched). An Interchange's
    // 12 SEMI_CV × attenuverter drive its targetHalf's 12 degrees; multiple on the same half SUM then
    // clamp. DISCOVERY is control-rate (rack-wide getModuleIds() every sample is the CA CPU pitfall):
    // rebuild the bound-Interchange cache on a divider, read live CV every sample from the cache.
    if (ixScanDiv_.getDivision() == 0) ixScanDiv_.setDivision(64);
    if (ixScanDiv_.process()) {
        boundInterchanges_.clear();
        boundShophouseMicro_ = nullptr;      // also (re)discover the bound Shophouse Micro scene source
        if (APP && APP->engine) {
            for (int64_t id : APP->engine->getModuleIds()) {
                rack::Module* m = APP->engine->getModule(id);
                if (auto* ix = dynamic_cast<MonsoonInterchangeExpander*>(m)) {
                    const bool bound = (ix->followTarget > 0) ? (ix->followTarget == pairId)
                                     : (redDot::findPairHubEitherSide<MicroTuningModule>(ix) == this);
                    if (bound) boundInterchanges_.push_back(ix);
                } else if (auto* sm = dynamic_cast<MonsoonShophouseMicro*>(m)) {
                    // First Shophouse Micro whose nearest host is OUR Monsoon (adjacency binding).
                    if (!boundShophouseMicro_ && redDot::findMonsoonEitherSide(sm) == mon)
                        boundShophouseMicro_ = sm;
                }
            }
        }
    }
    // EDGE RULING (Rodney): ONE active Interchange PER HALF, extras inert — NOT summed. Summing
    // saturated unpredictably at 3+/half (each add clamps at 1) and depended on discovery order. The
    // first-bound Interchange claims a half; later ones on that half are ignored. This gives, uniformly:
    //   • Colonnades Duo (24 deg): at most TWO effective — one on half 1 (deg 0..11), one on half 2
    //     (deg 12..23) = the intended cooperative pair.
    //   • Colonnades (12 deg): at most ONE effective — only half 1 exists (half 2's degrees ≥12 don't),
    //     so a 12-Micro is capped at a single Interchange however many are attached.
    // boundInterchanges_ is built by the rack-wide scan above in getModuleIds() order → a stable winner.
    bool halfClaimed[2] = { false, false };
    for (rack::Module* m : boundInterchanges_) {
        auto* ix = dynamic_cast<MonsoonInterchangeExpander*>(m);
        if (!ix) continue;
        const int halfIdx = (ix->targetHalf == 2) ? 1 : 0;
        const int base    = halfIdx * 12;                   // which 12 degrees this Interchange drives
        if (base >= n)        continue;                     // half 2 on a 12-Micro: no such degrees → inert
        if (halfClaimed[halfIdx]) continue;                 // a second Interchange on this half → inert
        halfClaimed[halfIdx] = true;                        // this one owns the half
        for (int k = 0; k < 12; ++k) {
            const int deg = base + k;
            if (deg >= n) break;
            int inId  = MonsoonIds::EXPANDER_SEMI_CV_INPUT_0 + k;
            int attId = MonsoonIds::EXPANDER_SEMI_ATTENUVERTER_0 + k;
            if (inId < (int)ix->inputs.size() && ix->inputs[inId].isConnected()) {
                float att = (attId < (int)ix->params.size()) ? ix->params[attId].getValue() : 1.f;
                tt.weight[deg] = clamp(tt.weight[deg] + (ix->inputs[inId].getVoltage() * att) / 10.f, 0.f, 1.f);
            }
        }
    }
    // CONSERVATION (Rodney, matches the original Shophouse): the bound Shophouse Micro's Guide/Enforce
    // toggle governs whether the scale MASK (deactivated in-tuning degrees, from an active FRONT OR the
    // Colonnades' own enable band) is an audible gate or a purely-visual guide. Governs the rig WHENEVER
    // a Shophouse Micro is bound — not only when a front is active. No Shophouse Micro bound → no
    // conservation control → the Colonnades mask stays hard (byte-identical to standalone).
    //   GUIDE (enforce off): SCALE GUIDE ONLY — deactivated (out-of-scale) degrees within the tuning
    //     STILL SOUND at their weight fader + any Interchange modulation. Un-gate the engine by forcing
    //     tt.enabled=true across the LIVE degrees; the fader still DIMS (the dim reads enabledState, not
    //     tt.enabled) so the scale stays visible.
    //   ENFORCE (conservation): the mask is HARD — out-of-scale degrees are silent EVEN WITH modulation.
    //     tt.enabled already carries the mask; ModeController + the quantizer zero a !enabled degree at
    //     read regardless of its weight/mod, so nothing more to do here.
    if (sceneGuide)
        for (int i = 0; i < tt.N; ++i) tt.enabled[i] = true;   // Guide: un-gate; dimming stays a guide
    // Mod-viz snapshot for the fader arcs (Interchange CV deviation from the fader's set value). When a
    // Shophouse Micro SCENE is active it replaces the whole mask wholesale (not a per-fader nudge) — the
    // scene is shown on the Shophouse Micro panel, so suppress per-fader arcs to avoid every fader
    // lighting an arc. Arcs remain the Interchange-CV indicator when no scene is active.
    for (int i = 0; i < n; ++i) {
        modWeight[i] = tt.weight[i];
        modActive[i] = !sceneActive && std::fabs(tt.weight[i] - params[weightParam(i)].getValue()) > 1e-4f;
        // Fader DIM reads the scene/authored MASK (enabledState), NOT tt.enabled — under GUIDE tt.enabled
        // was un-gated to all-true, but the dim must still show the scale as a VISUAL GUIDE (original
        // Shophouse behaviour). enabledState holds the scene mask (driven in) or the authored mask.
        effectiveEnabled[i] = enabledState[i];
    }
    tt.maskAuthored = true;                    // Micro owns the scale mask this block (Model A)
    tt.recomputeDefaultFlag();                 // cents fast-path (weight doesn't affect the cents map)

    // Fader FLASH lights: the degree that PLAYS lights its fader — read the host's per-degree play
    // brightness (max over mono + active poly voices) and drive the RED sub-light (2i+1), green (2i)
    // at 0, matching Monsoon's SEMI_LED convention. Since the Micro owns weight[], "which degree
    // plays" is authored here, so its own fader is the right place to show it.
    for (int i = 0; i < n; ++i) {
        float b = (i < tt.N) ? mon->engine.gs.semiLedBrightness(i) : 0.f;   // greyed (>= tuning size): no flash
        for (int v = 0; v < mon->engine.numPolyVoices && i < tt.N; ++v)
            b = std::max(b, mon->engine.voices[v].gs.semiLedBrightness(i));
        lights[weightLed(i) + 0].setBrightness(0.f);   // green sub unused (match Monsoon)
        lights[weightLed(i) + 1].setBrightness(b);     // red sub = play flash
    }
}

// ── MicroLightSlider — weight fader that DIMS when its degree is disabled (weight==0), reusing
// Monsoon's dim-out-of-scale idiom, and draws an Interchange MOD-ARC marker (3C-ii) at the modulated
// weight, mirroring Monsoon's semitone arcs. Display-only: reads state, never writes. ──────────────
template <typename TLightBase = RedLight>
struct MicroLightSlider : VCVLightSlider<TLightBase> {
    // Degree index of this fader (WEIGHT block starts at 0). -1 if not resolvable.
    int degOf() {
        auto* pq = this->getParamQuantity();
        auto* mod = pq ? dynamic_cast<MicroTuningModule*>(pq->module) : nullptr;
        if (!mod) return -1;
        int deg = pq->paramId - microTuning::weightParam(0);
        return (deg < 0 || deg >= mod->nDegrees) ? -1 : deg;
    }
    // GREYED = beyond the TUNING SIZE (deg >= liveN, ROUND 10): not part of the tuning at all. Distinct
    // from dimmed (in-tuning, out-of-scale). Heavier fade than dim.
    bool degreeGreyed() {
        auto* pq = this->getParamQuantity();
        auto* mod = pq ? dynamic_cast<MicroTuningModule*>(pq->module) : nullptr;
        int deg = degOf();
        return mod && deg >= 0 && deg >= mod->liveN();
    }
    bool degreeDisabled() {
        // DIM = OUT OF SCALE (enabled=false), NOT weight==0 (a fader at 0 is in-scale + raisable now).
        auto* pq = this->getParamQuantity();   // non-const (Rack API), so this method isn't const
        auto* mod = pq ? dynamic_cast<MicroTuningModule*>(pq->module) : nullptr;
        int deg = degOf();
        if (!mod || deg < 0) return false;
        // Read the EFFECTIVE mask (post Shophouse Micro override), not the base enabledState[],
        // so faders dim under an active scene override, not only under the Colonnades' own authoring.
        return !mod->effectiveEnabled[deg];
    }
    void draw(const widget::Widget::DrawArgs& args) override {
        const bool greyed = degreeGreyed();
        const bool dimmed = !greyed && degreeDisabled();
        if (greyed)      nvgGlobalAlpha(args.vg, 0.15f);   // beyond N: heavily greyed (not in the tuning)
        else if (dimmed) nvgGlobalAlpha(args.vg, 0.4f);    // out-of-scale within the tuning
        VCVLightSlider<TLightBase>::draw(args);
        if (greyed || dimmed) nvgGlobalAlpha(args.vg, 1.0f);
        if (!greyed) drawModArc(args);                     // no mod-arc on a greyed (beyond-tuning) fader
    }
    // Blue arc marker at the Interchange-modulated weight (same look as MonsoonWidget's semitone arc).
    void drawModArc(const widget::Widget::DrawArgs& args) {
        auto* pq = this->getParamQuantity();
        auto* mod = pq ? dynamic_cast<MicroTuningModule*>(pq->module) : nullptr;
        if (!mod) return;
        const int deg = pq->paramId - microTuning::weightParam(0);   // WEIGHT block starts at 0
        if (deg < 0 || deg >= mod->nDegrees) return;
        if (!mod->modActive[deg]) return;
        float modN = rack::math::clamp(mod->modWeight[deg], 0.f, 1.f);
        float top = this->box.size.y * 0.10f, bot = this->box.size.y * 0.90f;
        float y = top + (1.f - modN) * (bot - top);
        float cx = this->box.size.x * 0.5f;
        nvgBeginPath(args.vg); nvgCircle(args.vg, cx, y, 4.2f);
        nvgFillColor(args.vg, nvgRGBAf(0.1f, 0.8f, 0.95f, 0.30f)); nvgFill(args.vg);   // halo
        nvgBeginPath(args.vg); nvgCircle(args.vg, cx, y, 2.2f);
        nvgFillColor(args.vg, nvgRGBAf(0.4f, 0.95f, 1.0f, 0.95f)); nvgFill(args.vg);   // core
    }
};

// ── Widget-drawn labels (nanosvg ignores <text>): wordmark + subtitle + per-degree numbers. ──────
struct MicroLabels : Widget {
    MicroTuningModule* module = nullptr;
    std::string wordmark;
    Vec wordmarkPos;
    std::vector<Vec> cellPos;   // per-degree number anchor (sized nDegrees)

    std::shared_ptr<window::Font> loadFont() {
        auto f = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!f) f = APP->window->uiFont;
        return f;
    }
    void draw(const DrawArgs& args) override {
        Widget::draw(args);
        auto f = loadFont();
        if (!f) return;
        NVGcontext* vg = args.vg;
        const bool light = [&]{ Monsoon* m = module ? redDot::findMonsoonEitherSide(module) : nullptr;
                                return m && m->lightTheme; }();
        const NVGcolor ink = light ? nvgRGB(0x1a,0x1a,0x1a) : nvgRGB(0xf0,0xf0,0xf0);
        const NVGcolor sub = light ? nvgRGB(0x5a,0x64,0x70) : nvgRGB(0x8a,0x94,0xa0);
        nvgFontFaceId(vg, f->handle);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFontSize(vg, 15.f);
        nvgFillColor(vg, ink);
        nvgText(vg, wordmarkPos.x, wordmarkPos.y - 4.f, wordmark.c_str(), nullptr);
        nvgFontSize(vg, 6.f);
        nvgFillColor(vg, sub);
        nvgText(vg, wordmarkPos.x, wordmarkPos.y + 6.f, "tuning + scale", nullptr);
        nvgFontSize(vg, 8.f);
        nvgFillColor(vg, ink);
        for (size_t i = 0; i < cellPos.size(); ++i)
            nvgText(vg, cellPos[i].x, cellPos[i].y, std::to_string((int)i + 1).c_str(), nullptr);
    }
};

// ── MicroCentsDisplay — LED cents readout. Two layouts (subclass hook mtCentsSingleRow):
//   • two-row staggered (Colonnades, ROUND 7): Scalar's grid, lower row offset 50% in X — even
//     degrees upper, odd lower, contiguous cells.
//   • single row (Colonnades Duo, 24): one row of contiguous cells, all degrees at one Y.
// Reads cents params live (root shows 0.00). Owner sets faderX[] + row Ys + cell half-extents.
struct MicroCentsDisplay : Widget {
    MicroTuningModule* module = nullptr;
    int   n = 12;
    bool  singleRow = false;
    std::vector<float> faderX;                  // per-degree centre X (display-local), sized n
    float rowUpperY = 0.f, rowLowerY = 0.f;     // even / odd row centre Y (two-row); single row uses Upper
    float cellHalfW = 0.f, cellHalfH = 0.f;

    std::shared_ptr<window::Font> ledFont() {
        auto f = APP->window->loadFont(rack::asset::plugin(pluginInstance, "res/fonts/DSEG7ClassicMini-Bold.ttf"));
        if (!f) f = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!f) f = APP->window->uiFont;
        return f;
    }
    float rowYof(int i) const { return (singleRow || (i % 2 == 0)) ? rowUpperY : rowLowerY; }

    void draw(const DrawArgs& args) override {
        Widget::draw(args);
        auto f = ledFont();
        if (!f) return;
        NVGcontext* vg = args.vg;

        // Contiguous grid cells (stroked rectangles). Colonnades: two rows, lower offset 50% in X (the
        // odd faders are half a pitch off the even → the shared horizontal divider steps). Duo: one row.
        // ROUND 10: only draw cells for LIVE degrees (i < tuning size). Greyed degrees (beyond N) have no
        // cents cell. `liveCount` = the tuning size (falls back to n when standalone).
        const int liveCount = module ? module->liveN() : n;
        nvgStrokeColor(vg, nvgRGBA(0x8a, 0x94, 0xa0, 0x55));
        nvgStrokeWidth(vg, 0.8f);
        for (int i = 0; i < liveCount && i < n && i < (int)faderX.size(); ++i) {
            const float cx = faderX[i];
            const float cy = rowYof(i);
            nvgBeginPath(vg);
            nvgRect(vg, cx - cellHalfW, cy - cellHalfH, 2*cellHalfW, 2*cellHalfH);
            nvgStroke(vg);
        }

        nvgFontFaceId(vg, f->handle);
        nvgFontSize(vg, singleRow ? 8.5f : 11.5f);   // 24 across one row needs a smaller face
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        // Reads the cents KNOB params live. A Shophouse Micro scene DRIVES those params directly (and
        // restores them on detach), so the LED follows the modulation automatically — no separate table
        // read needed. Root shows 0.00.
        for (int i = 0; i < liveCount && i < n && i < (int)faderX.size(); ++i) {   // greyed degrees: no value
            const float cx = faderX[i];
            const float cy = rowYof(i);
            double cents = module ? (double)module->params[centsParam(n, i)].getValue()
                                  : (double)defaultCents(i, n);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.2f", cents);
            nvgFillColor(vg, nvgRGBA(0x40, 0x18, 0x00, 0x55));   // dim amber ghost (off-segments)
            nvgText(vg, cx, cy, "888.88", nullptr);
            nvgFillColor(vg, nvgRGB(0xff, 0x9a, 0x2a));          // lit amber
            nvgText(vg, cx, cy, buf, nullptr);
        }
    }
};

// ── MicroNotesControl — the NUMBER control = TUNING SIZE (ROUND 10). Displays tuningN (how many faders
// are LIVE, 1..capacity); vertical drag sets it. This is DECOUPLED from the enabled[] mask (the enable
// band is the sole enabled[] writer now): shrinking greys the faders >= N (they leave the tuning);
// growing makes them live and defaults the newly-live degrees enabled=true. Weight faders untouched. ──
struct MicroNotesControl : OpaqueWidget {
    MicroTuningModule* module = nullptr;
    float accum = 0.f;

    static int activeCount(MicroTuningModule* m) {   // now the TUNING SIZE readout (not enabled count)
        return m ? m->liveN() : 0;
    }
    static void setActiveCount(MicroTuningModule* m, int N) {
        if (!m) return;
        N = clamp(N, 1, m->nDegrees);
        const int old = m->tuningN;
        m->tuningN = N;
        for (int i = old; i < N; ++i)       // newly-live degrees (on grow) default in-scale
            m->enabledState[i] = true;
        m->enabledState[0] = true;          // root always in-scale
        // Shrink leaves enabledState[>=N] untouched (greyed; preserved so a re-grow restores the mask).
    }

    std::shared_ptr<window::Font> ledFont() {
        auto f = APP->window->loadFont(rack::asset::plugin(pluginInstance, "res/fonts/DSEG7ClassicMini-Bold.ttf"));
        if (!f) f = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!f) f = APP->window->uiFont;
        return f;
    }
    void draw(const DrawArgs& args) override {
        auto f = ledFont();
        if (!f) return;
        NVGcontext* vg = args.vg;
        const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        nvgFontFaceId(vg, f->handle);
        nvgFontSize(vg, 10.f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", activeCount(module));
        nvgFillColor(vg, nvgRGBA(0x40, 0x18, 0x00, 0x55));
        nvgText(vg, cx, cy, "88", nullptr);
        nvgFillColor(vg, nvgRGB(0xff, 0x9a, 0x2a));
        nvgText(vg, cx, cy, buf, nullptr);
    }
    void onDragStart(const event::DragStart&) override { accum = 0.f; }
    void onDragMove(const event::DragMove& e) override {
        if (!module) return;
        accum -= e.mouseDelta.y;               // up = more notes, ~10px/step
        int step = (int)(accum / 10.f);
        if (step != 0) {
            setActiveCount(module, activeCount(module) + step);
            accum -= step * 10.f;
        }
    }
};

// ── MicroEnableBand — the enable/disable gesture strip behind the number row (ENABLED_MASK_BUILD_BRIEF
// §5b). Single click toggles a degree's enabledState; horizontal drag PAINTS a range to ONE state set
// by the first degree touched (start-on-enabled → disable swept, else enable). Root (0) is always
// enabled: paint skips it, click on it is a no-op. Maps local X → degree via the fader-centre list. ─
struct MicroEnableBand : OpaqueWidget {
    MicroTuningModule* module = nullptr;
    std::vector<float> faderX;    // fader centres, box-local px (index = degree)
    int   paintState = -1;        // -1 none; 0 painting-disable; 1 painting-enable
    int   lastDeg    = -1;
    float dragX      = 0.f;       // box-local pointer X, seeded at press + accumulated by drag deltas

    int degreeAt(float xLocal) const {
        int best = -1; float bestD = 1e9f;
        for (int i = 0; i < (int)faderX.size(); ++i) {
            float d = std::fabs(xLocal - faderX[i]);
            if (d < bestD) { bestD = d; best = i; }
        }
        // ROUND 10: the enable band can only mask WITHIN the tuning (0..liveN-1). A hit beyond N is a
        // greyed/not-in-tuning degree → not editable here (return -1).
        if (module && best >= module->liveN()) return -1;
        return best;
    }
    void onButton(const event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
            dragX = e.pos.x;                          // seed for a possible drag-paint
            int deg = degreeAt(e.pos.x);
            if (deg > 0) module->enabledState[deg] = !module->enabledState[deg];   // single-click toggle
            // (click on root = no-op). A drag that follows repaints via onDragMove.
            e.consume(this);
            return;
        }
        OpaqueWidget::onButton(e);
    }
    void onDragStart(const event::DragStart&) override { paintState = -1; lastDeg = -1; }
    void onDragMove(const event::DragMove& e) override {
        if (!module) return;
        dragX += e.mouseDelta.x;                      // accumulate pointer X in box-local space
        int deg = degreeAt(dragX);
        if (deg < 0) return;
        if (paintState < 0) {
            if (deg == 0) return;                     // root can't seed a paint direction
            // The click in onButton already flipped the seed degree; paint the REST of the range to
            // THAT degree's (now-current) state, so one gesture builds or clears a contiguous scale.
            paintState = module->enabledState[deg] ? 1 : 0;
            lastDeg = deg;
            return;
        }
        if (deg != lastDeg) {
            if (deg > 0) module->enabledState[deg] = (paintState != 0);   // root always skipped
            lastDeg = deg;
        }
    }
    void onDragEnd(const event::DragEnd&) override { paintState = -1; lastDeg = -1; }
};

// ── Widget base: shared ctor body ───────────────────────────────────────────────────────────────
void MicroTuningWidget::build() {
    const int n = mtDegrees();
    panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, mtPanelDark()));
    panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, mtPanelLight()));
    loadPanel(asset::plugin(pluginInstance, mtPanelDark()));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // Per-degree strips: WEIGHT light-fader (all n; GreenRedLight flashes red on play, dimmed when
    // weight==0) + CENTS knob (degrees 1..n-1; root cents locked, no knob).
    for (int i = 0; i < n; ++i)
        bindLightParam<MicroLightSlider<GreenRedLight>>(
            "param_weight_" + std::to_string(i), weightParam(i), weightLed(i));
    for (int i = 1; i < n; ++i)
        bindParam<Trimpot>("param_cents_" + std::to_string(i), centsParam(n, i));

    // Cents LED display — layout per mtCentsSingleRow(). Capture each fader's X (display-local).
    if (auto* dispShape = findNamed("cents_display")) {
        auto* disp = new MicroCentsDisplay();
        disp->module    = dynamic_cast<MicroTuningModule*>(module);
        disp->n         = n;
        disp->singleRow = mtCentsSingleRow();
        Rect db = boundsOf(dispShape);
        disp->box.pos  = db.pos;
        disp->box.size = db.size;
        disp->faderX.assign(n, 0.f);
        for (int i = 0; i < n; ++i)
            if (auto* w = findNamed("param_weight_" + std::to_string(i)))
                disp->faderX[i] = centerOf(w).x - db.pos.x;
        if (disp->singleRow) {
            // One row of 24: cells half-width = half the fader pitch so adjacent cells tile edge-to-
            // edge; centred vertically in the band.
            float pitchPx = (n > 1) ? std::fabs(disp->faderX[1] - disp->faderX[0]) : mm2px(4.5f);
            disp->cellHalfW = pitchPx * 0.5f;
            disp->cellHalfH = db.size.y * 0.42f;
            disp->rowUpperY = db.size.y * 0.5f;
            disp->rowLowerY = db.size.y * 0.5f;
        } else {
            // Two staggered rows (ROUND 7): half-width = one full fader pitch so same-row cells (2
            // pitches apart) tile edge-to-edge; rows adjacent, sharing a stepped divider.
            disp->cellHalfW = mm2px(9.0f);
            disp->cellHalfH = db.size.y * 0.25f;
            disp->rowUpperY = db.size.y * 0.25f;   // even degrees (upper)
            disp->rowLowerY = db.size.y * 0.75f;   // odd degrees (lower, +half-column in X)
        }
        addChild(disp);
    }

    // NOTES control — zero-state readout/bulk-set of active-degree count.
    if (auto* nShape = findNamed("notes_ctrl")) {
        auto* nc = new MicroNotesControl();
        nc->module = dynamic_cast<MicroTuningModule*>(module);
        Rect nb = boundsOf(nShape);
        nc->box.pos  = nb.pos;
        nc->box.size = nb.size;
        addChild(nc);
    }

    // ENABLE BAND — click/swipe-paint per-degree scale membership (ENABLED_MASK_BUILD_BRIEF §5b).
    // Local X → degree via the fader centres so hits line up with the number strip.
    if (auto* bShape = findNamed("enable_band")) {
        auto* eb = new MicroEnableBand();
        eb->module = dynamic_cast<MicroTuningModule*>(module);
        Rect bb = boundsOf(bShape);
        eb->box.pos  = bb.pos;
        eb->box.size = bb.size;
        eb->faderX.assign(n, 0.f);
        for (int i = 0; i < n; ++i)
            if (auto* w = findNamed("param_weight_" + std::to_string(i)))
                eb->faderX[i] = centerOf(w).x - bb.pos.x;   // band-local
        addChild(eb);
    }

    // Widget-drawn labels overlay.
    {
        auto* lab = new MicroLabels();
        lab->module = dynamic_cast<MicroTuningModule*>(module);
        lab->wordmark = mtWordmark();
        lab->box.pos = Vec(0, 0);
        lab->box.size = box.size;
        lab->cellPos.assign(n, Vec(0, 0));
        if (auto* wm = findNamed("wordmark")) lab->wordmarkPos = centerOf(wm);
        for (int i = 0; i < n; ++i)
            if (auto* c = findNamed("notelabel_" + std::to_string(i))) lab->cellPos[i] = centerOf(c);
        labels = lab;
        addChild(lab);
    }

    // ConnectMark: lights only when THIS Micro is the claimed tuning source.
    if (auto* s = findNamed("light_connect")) {
        connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
        rack::Module* self = module;
        connectMark->connected = [self]() {
            Monsoon* m = self ? redDot::findMonsoonEitherSide(self) : nullptr;
            return m && m->getTuningSourceExpander() == self;
        };
        addChild(connectMark);
    }
}

void MicroTuningWidget::openScalaFilePicker() {
    auto* mod = dynamic_cast<MicroTuningModule*>(module);
    if (!mod) return;
    const int n = mod->nDegrees;
    osdialog_filters* filters = osdialog_filters_parse("Scala Tuning:scl");
    char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
    osdialog_filters_free(filters);
    if (!path) return;
    std::string pathStr(path);
    std::free(path);

    // UP TO n degrees (a shorter .scl fills the first N degrees + disables the rest).
    auto sf = dotModular::loadScala(pathStr,
        [n](int cnt){ return cnt >= 1 && cnt <= n; },
        "This tuning has more tones than this expander supports.");
    if (!sf.ok()) {
        osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, sf.errorMessage.c_str());
        return;
    }
    const int cnt = sf.degreeCount();
    // Scala convention: the LAST pitch is the PERIOD (octave) — dropped; within-octave non-root degrees
    // are the first (cnt-1) listed pitches. ROUND 10: a .scl is NOTES-based — its length SETS the TUNING
    // SIZE (tuningN = cnt); degrees cnt..capacity-1 grey out (beyond the tuning). Loaded degrees default
    // in-scale (enabled). WEIGHT faders untouched (a scale masks, it never moves your faders).
    mod->tuningN = cnt < 1 ? 1 : (cnt > n ? n : cnt);
    mod->params[centsParam(n, 0)].setValue(0.f);
    mod->enabledState[0] = true;
    const int nonRoot = (cnt > 0) ? (cnt - 1) : 0;
    for (int deg = 1; deg < n; ++deg) {
        if (deg <= nonRoot) {
            mod->params[centsParam(n, deg)].setValue(sf.centsFromRoot[deg - 1]);
            mod->enabledState[deg] = true;
        }
        // deg >= cnt: greyed (beyond the tuning) — leave authored cents/enabled untouched.
    }
    std::string nm = sf.description;
    if (nm.empty()) {
        size_t slash = pathStr.find_last_of("/\\");
        std::string base = (slash == std::string::npos) ? pathStr : pathStr.substr(slash + 1);
        size_t dot = base.find_last_of('.');
        nm = (dot == std::string::npos) ? base : base.substr(0, dot);
    }
    mod->loadedTuningName = nm;
}

void MicroTuningWidget::openScalaSavePicker() {
    auto* mod = dynamic_cast<MicroTuningModule*>(module);
    if (!mod) return;
    const int n = mod->nDegrees;
    osdialog_filters* filters = osdialog_filters_parse("Scala Tuning:scl");
    char* path = osdialog_file(OSDIALOG_SAVE, nullptr, "tuning.scl", filters);
    osdialog_filters_free(filters);
    if (!path) return;
    std::string pathStr(path);
    std::free(path);
    if (pathStr.size() < 4 || pathStr.substr(pathStr.size() - 4) != ".scl")
        pathStr += ".scl";

    // .scl = the TUNING, sized by the TUNING SIZE (liveN, ROUND 10). Export the non-root LIVE degrees'
    // cents, MASK-INDEPENDENT (a masked-out but in-tuning degree still exports — ENABLED_MASK_BUILD_BRIEF
    // §53). Greyed degrees (>= liveN) are beyond the tuning and NOT exported.
    std::vector<float> cents;
    const int nv = mod->liveN();
    for (int deg = 1; deg < nv; ++deg)
        cents.push_back(mod->params[centsParam(n, deg)].getValue());
    cents.push_back(1200.f);   // period (octave)

    std::string desc = mod->loadedTuningName.empty() ? std::string(mtWordmark()) + " tuning"
                                                      : mod->loadedTuningName;
    if (!dotModular::saveScala(pathStr, cents, desc))
        osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, ("Could not write file: " + pathStr).c_str());
}

void MicroTuningWidget::openDmtuneLoadPicker() {
    auto* mod = dynamic_cast<MicroTuningModule*>(module);
    if (!mod) return;
    const int n = mod->nDegrees;
    osdialog_filters* filters = osdialog_filters_parse("dot.modular Tuning:dmtune");
    char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
    osdialog_filters_free(filters);
    if (!path) return;
    std::string pathStr(path);
    std::free(path);
    // ROUND 10 full model: accept ANY 1..capacity (Colonnades 1..12, Duo 1..24 including N≤12). The
    // file's n SETS the tuning size; degrees n..capacity-1 grey out.
    auto p = dotModular::loadTuningPreset(pathStr,
        [n](int cnt){ return cnt >= 1 && cnt <= n; },
        "This .dmtune has more degrees than this expander's capacity.");
    if (!p.ok()) {
        osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, p.errorMessage.c_str());
        return;
    }
    // .dmtune v2 sets cents + scale MEMBERSHIP (enabledState) + TUNING SIZE (tuningN = file n); WEIGHT
    // faders untouched (the preset has no weight). Root always enabled. Degrees >= p.n grey out.
    mod->tuningN = p.n < 1 ? 1 : (p.n > n ? n : p.n);
    for (int i = 0; i < n; ++i) {
        if (i < p.n) {
            mod->params[centsParam(n, i)].setValue(i == 0 ? 0.f : p.cents[i]);
            mod->enabledState[i] = (i == 0) ? true : p.enabled[i];
        }
        // i >= p.n: greyed (beyond the tuning) — leave the authored cents/enabled untouched.
    }
    mod->loadedTuningName = !p.name.empty() ? p.name : std::string();
}

void MicroTuningWidget::openDmtuneSavePicker() {
    auto* mod = dynamic_cast<MicroTuningModule*>(module);
    if (!mod) return;
    const int n = mod->nDegrees;
    osdialog_filters* filters = osdialog_filters_parse("dot.modular Tuning:dmtune");
    char* path = osdialog_file(OSDIALOG_SAVE, nullptr, "tuning.dmtune", filters);
    osdialog_filters_free(filters);
    if (!path) return;
    std::string pathStr(path);
    std::free(path);
    if (pathStr.size() < 7 || pathStr.substr(pathStr.size() - 7) != ".dmtune")
        pathStr += ".dmtune";
    // ROUND 10 bug fix: write the TUNING SIZE (liveN), NOT the capacity. Degrees >= liveN are beyond the
    // tuning and not written — the file's n IS the tuning size.
    dotModular::TuningPreset p;
    const int nv = mod->liveN();
    p.n = nv;
    for (int i = 0; i < nv; ++i) {
        p.cents[i]   = mod->params[centsParam(n, i)].getValue();
        p.enabled[i] = mod->enabledState[i];   // v2: scale mask, not the loudness faders
    }
    p.name = mod->loadedTuningName;
    if (!dotModular::saveTuningPreset(pathStr, p))
        osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, ("Could not write file: " + pathStr).c_str());
}

void MicroTuningWidget::appendContextMenu(Menu* menu) {
    ModuleWidget::appendContextMenu(menu);
    menu->addChild(new MenuSeparator);
    // Pair number: Interchange expanders bind to this Colonnades/Duo by this id (3C-ii). Show it so the
    // user can pick the matching "Follow: <name> #N" on the Interchange.
    if (auto* mod = dynamic_cast<MicroTuningModule*>(module); mod && mod->pairId > 0)
        menu->addChild(createMenuLabel("Pair #" + std::to_string(mod->pairId) + " (Interchange follows this)"));
    if (auto* mod = dynamic_cast<MicroTuningModule*>(module); mod && !mod->loadedTuningName.empty())
        // WRAPPING label (SCALA_FILE_AND_LOAD_UI.md §BUG+FIX): a long .scl description would otherwise
        // blow out the whole menu width (VCV sizes a menu to its widest label). This wraps to a fixed
        // width instead, so the full name stays visible without an absurd menu.
        menu->addChild(redDot::makeWrappingMenuLabel("Loaded: " + mod->loadedTuningName));
    menu->addChild(createMenuItem("Load .scl...", "", [this]() { this->openScalaFilePicker(); }));
    menu->addChild(createMenuItem("Save .scl...", "", [this]() { this->openScalaSavePicker(); }));
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuItem("Load .dmtune... (full state)", "", [this]() { this->openDmtuneLoadPicker(); }));
    menu->addChild(createMenuItem("Save .dmtune... (full state)", "", [this]() { this->openDmtuneSavePicker(); }));
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuItem("Reset to equal division (all degrees enabled)", "", [this]() {
        if (auto* mod = dynamic_cast<MicroTuningModule*>(module)) {
            const int n = mod->nDegrees;
            for (int i = 0; i < n; ++i) {
                mod->params[centsParam(n, i)].setValue(defaultCents(i, n));
                mod->params[weightParam(i)].setValue(1.f);
            }
            mod->loadedTuningName.clear();
        }
    }));
    // Set equal-tempered across the TUNING (N-EDO seed, ROUND 10): N = the tuning size (liveN). Write
    // cents for every LIVE degree (i < liveN), independent of loudness or the enable mask — this seeds
    // the TUNING, not the scale. Greyed degrees (>= liveN) are untouched.
    menu->addChild(createMenuItem("Set equal-tempered (N divisions)", "", [this]() {
        if (auto* mod = dynamic_cast<MicroTuningModule*>(module)) {
            const int n = mod->nDegrees;
            const int N = mod->liveN();
            for (int i = 0; i < N && i < n; ++i)
                mod->params[centsParam(n, i)].setValue((float)i * (1200.f / (float)N));
            mod->params[centsParam(n, 0)].setValue(0.f);   // root always 0
        }
    }));
}

void MicroTuningWidget::step() {
    ModuleWidget::step();
    kitStep();
    if (!module) return;
    Monsoon* m = redDot::findMonsoonEitherSide(module);
    int wantLight = (m && m->lightTheme) ? 1 : 0;
    if (wantLight != lastThemeLight) {
        lastThemeLight = wantLight;
        for (Widget* child : children) {
            if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                sp->setBackground(wantLight ? panelSvgLight : panelSvgDark);
                break;
            }
        }
    }
}
