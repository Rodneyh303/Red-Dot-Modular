#include <rack.hpp>
#include "ColonnadesDuo.hpp"

using namespace rack;

// ── Colonnades Duo widget — thin subclass of MicroTuningWidget (Option B shared base). Supplies only
// the THREE differences from Colonnades: degree count (24), panel paths, wordmark. The cents readout
// stays the SAME two-row staggered ROUND-7 grid — the Duo panel IS gen_colonnades.py at N=24, so the
// display cells + knob stagger are identical, just 24 of them (COLONNADES_DUO_PANEL_SPEC.md). ──────
struct ColonnadesDuoWidget : MicroTuningWidget {
    int         mtDegrees()        const override { return ColonnadesDuoIds::N_DEGREES; }   // 24
    const char* mtPanelDark()      const override { return "res/panels/ColonnadesDuo_panel_dark.svg"; }
    const char* mtPanelLight()     const override { return "res/panels/ColonnadesDuo_panel_light.svg"; }
    const char* mtWordmark()       const override { return "Colonnades Duo"; }
    bool        mtCentsSingleRow() const override { return false; }  // two-row staggered (same as Colonnades)

    explicit ColonnadesDuoWidget(ColonnadesDuo* mod) {
        setModule(mod);
        build();
    }
};

Model* modelColonnadesDuo = createModel<ColonnadesDuo, ColonnadesDuoWidget>("ColonnadesDuo");
