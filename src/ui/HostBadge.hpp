#pragma once
// ── HostBadge — the ONE shared connection-identity badge (CONNECTION_MODEL_SPEC.md §4) ────────────
// A small coloured disc + number that identifies WHICH host Monsoon a module is bound to, drawn
// identically on every participating module so a binding is verified by matching two identical badges
// across the rack (the pairId/pairColour convention — IntertropicalPairing.hpp).
//
// Fixed vocabulary (§4):
//   • colour  = pairColour(hostPairId)   — the 8-hue palette; unique per host under the 8-cap.
//   • number  = hostPairId               — colour-blind fallback; never needed for uniqueness.
//   • filled vs hollow = primary vs secondary — ONLY meaningful for shareable mutators reached by 2+
//     hosts (CA today). Every other module is always "filled" (there is no secondary role).
//
// SUPPRESSION (§4 / Q6): the badge draws NOTHING when fewer than 2 Monsoons exist in the patch — the
// common single-Monsoon rig shows only its plain ConnectMark, zero added noise. monsoonCount() is the
// gate; it is cheap (a getModuleIds walk) and callers already run at draw rate, but see the note on
// caching if a hot path needs it.
//
// Decoupled like ConnectMark: the widget holds std::function accessors, so it names no concrete module
// type. Bind: hostPairId() → the bound host's pairId (0 = unbound/unparticipating → no draw); filled()
// → primary (default true); lightTheme() → theme variant.

#include <rack.hpp>
#include "IntertropicalPairing.hpp"   // redDot::pairColour
#include "../Monsoon.hpp"             // Monsoon type + modelMonsoon (for the count gate)

namespace redDot {

// Number of Monsoons currently in the patch. The badge is suppressed below 2 (§4/Q6). Counts ALL
// Monsoons present, not only participating ones — a 9th (capped, pairId 0) still means "multi-Monsoon
// rig", so the badges on the other eight must show. (A capped host draws no badge itself since its
// hostPairId is 0.)
inline int monsoonCount() {
    int n = 0;
    if (APP && APP->engine) {
        for (int64_t id : APP->engine->getModuleIds()) {
            rack::Module* m = APP->engine->getModule(id);
            if (m && m->model == modelMonsoon) ++n;
        }
    }
    return n;
}

struct HostBadge : rack::widget::Widget {
    std::function<int()>  hostPairId;    // bound host's pairId; 0 => draw nothing
    std::function<bool()> filled;        // primary (filled) vs secondary (hollow ring); default primary
    std::function<bool()> lightTheme;    // theme variant for the number contrast
    float radiusPx = 0.f;                // 0 => derive from box

    void draw(const DrawArgs& args) override {
        // Suppress entirely in single-Monsoon rigs (§4/Q6): plain ConnectMark only.
        if (monsoonCount() < 2) return;
        const int id = hostPairId ? hostPairId() : 0;
        if (id <= 0) return;                          // unbound or capped-out host: no badge

        NVGcontext* vg = args.vg;
        const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        const float r  = radiusPx > 0.f ? radiusPx : std::min(box.size.x, box.size.y) * 0.5f;
        const NVGcolor col = redDot::pairColour(id);
        const bool prim = filled ? filled() : true;

        if (prim) {                                   // PRIMARY / sole host: filled disc
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, r);
            nvgFillColor(vg, col);
            nvgFill(vg);
        } else {                                       // SECONDARY (CA reader): hollow ring, same hue
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, r);
            nvgStrokeColor(vg, col);
            nvgStrokeWidth(vg, std::max(1.0f, r * 0.28f));
            nvgStroke(vg);
        }

        // Number — dark on a filled disc; the hue itself on a hollow ring (keeps contrast either way).
        char b[8]; snprintf(b, sizeof(b), "%d", id);
        nvgFontSize(vg, r * 1.5f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, prim ? nvgRGBA(0x0a, 0x0a, 0x0a, 0xff) : col);
        nvgText(vg, cx, cy + r * 0.06f, b, nullptr);
    }
};

// Build a HostBadge sized to `sizePx` (square) centred on panel coordinate `center`, reading the host
// pairId via the supplied getter. `primaryFn` defaults to always-primary (non-shareable modules); CA
// passes a real primary predicate. lightThemeFn matches the panel theme swap.
inline HostBadge* makeHostBadge(rack::math::Vec center, float sizePx,
                                std::function<int()> hostPairIdFn,
                                std::function<bool()> lightThemeFn,
                                std::function<bool()> primaryFn = nullptr) {
    auto* w = new HostBadge();
    w->box.size    = rack::math::Vec(sizePx, sizePx);
    w->box.pos     = center.minus(w->box.size.div(2));
    w->radiusPx    = sizePx * 0.5f;
    w->hostPairId  = hostPairIdFn;
    w->lightTheme  = lightThemeFn;
    w->filled      = primaryFn ? primaryFn : []() { return true; };
    return w;
}

}  // namespace redDot
