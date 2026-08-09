#include <rack.hpp>
#include "Colonnades.hpp"

using namespace rack;

// ── Colonnades widget — thin subclass of MicroTuningWidget (Option B shared base). Supplies only the
// three differences: degree count (12), panel paths, wordmark, and the two-row staggered cents grid
// (ROUND 7). Everything else — faders, cents knobs, LED display, NOTES control, ConnectMark, the four
// file ops, the context menu, theme switching — is the shared base. ─────────────────────────────
struct ColonnadesWidget : MicroTuningWidget {
    int         mtDegrees()        const override { return ColonnadesIds::N_DEGREES; }   // 12
    const char* mtPanelDark()      const override { return "res/panels/Colonnades_panel_dark.svg"; }
    const char* mtPanelLight()     const override { return "res/panels/Colonnades_panel_light.svg"; }
    const char* mtWordmark()       const override { return "Colonnades"; }
    bool        mtCentsSingleRow() const override { return false; }   // two staggered rows (ROUND 7)

    explicit ColonnadesWidget(Colonnades* mod) {
        setModule(mod);
        build();
    }
};

Model* modelColonnades = createModel<Colonnades, ColonnadesWidget>("Colonnades");
