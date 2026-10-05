// src/plugin.cpp — plugin entry point (Rack convention: a dedicated plugin.cpp beside the module
// sources, matching the Rack plugin template). Holds the Plugin* definition + init(Plugin*), the
// plugin's single entry point: registers every model and warms the Phi LUT on the load thread.
//
// Moved verbatim from src/Monsoon.cpp (where init() and `Plugin* pluginInstance` previously lived)
// so the plugin's entry point no longer depends on one module's translation unit, and touching a
// registration no longer rebuilds the large Monsoon.cpp. SOURCES += $(wildcard src/*.cpp) in the
// Makefile picks this up with no Makefile change.
//
// Registration vs plugin.json (reconciled 2026-09): 21 active addModel calls <-> 21 createModel
// slugs <-> 21 plugin.json module slugs — a perfect 1:1, no mismatch either way. The 7 deprecated
// models (ChangeAlleyExpander V1, Temasek, SandsExpander, StraitsSands, DeepStraitsSands East/West,
// StraitWest) are already fully retired: commented out below, absent from plugin.json, and
// src/deprecated/ is not compiled (the wildcard is non-recursive). Raffles ships (registered + slug).
// No deletions made in this move — the commented lines and the src/deprecated/ tree are retained.

#include <rack.hpp>
#include "Monsoon.hpp"               // extern Plugin* pluginInstance; + all extern Model* declarations
#include "dsp/GaussianCopula.hpp"    // redDot::copula::warmPhiLut()

using namespace rack;

Plugin* pluginInstance = nullptr;

void init(rack::Plugin* p) {
	pluginInstance = p;
	// Set plugin on UNREGISTERED models (base-spawned lane expanders) so
	// Module::toJson() can access model->plugin->slug during autosave.
	modelStraitsLaneQMIX->plugin = p;
	// Warm the Phi LUT on the load thread (~0.33 ms, once) so its one-time build never lands
	// mid-block on the audio thread at first spread/slew use. NOT in a module constructor —
	// multiple modules (Sands visuals, CA correlation) consume Phi. See SLEW_COPULA_PLAN.md.
	redDot::copula::warmPhiLut();
	p->addModel(modelMonsoon);
	p->addModel(modelMonsoonInterchangeExpander);
	p->addModel(modelMonsoonRafflesExpander);
	// DEPRECATED (§#7): superseded by Change Alley V2 (single module). Code retained.
	// p->addModel(modelMonsoonChangeAlleyExpander);
	p->addModel(modelMonsoonChangeAlleyV2);
	// DEPRECATED (§#7): folded into Change Alley V2. Code retained.
	// p->addModel(modelMonsoonTemasekExpander);
	p->addModel(modelMonsoonJunctionExpander);
	//p->addModel(modelMonsoonSandsExpander);
	p->addModel(modelMonsoonStraitsExpander);
	p->addModel(modelStraitsBase);             // lane-extension base (frame + IO + docking) — spawns lane expanders
	// StraitsLaneQMIX is UNREGISTERED — base-spawned only (not browser-draggable).
	// The Model* is kept for the base to instantiate via context menu.
	p->addModel(modelMonsoonCausewayPolyExpander);
	p->addModel(modelMonsoonChangiExpander);
	p->addModel(modelMonsoonChangiT2Expander);
	p->addModel(modelMonsoonChangiT3Expander);
	p->addModel(modelMonsoonShophouseExpander);
	p->addModel(modelSikit);                         // tuning expander (microtonal Phase 1)
	p->addModel(modelColonnades);                    // tuning+scale authoring (microtonal Phase 2)
	p->addModel(modelColonnadesDuo);                 // tuning+scale authoring, 24-tone (microtonal Phase 3)
	p->addModel(modelMonsoonShophouseMicro);         // tuning+scale scene modulator for Colonnades/Duo (.dmtune fronts)
	p->addModel(modelKeppel);                        // poly microtonal CV → MPE MIDI out (standalone utility)
	p->addModel(modelLantern);                       // Lantern note-output visualiser
	// West retired (Straits redesign): p->addModel(modelMonsoonStraitWestExpander);
	//p->addModel(modelMonsoonStraitsSands);          // Macro: global DNA
	//p->addModel(modelMonsoonDeepStraitsSandsEast);  // Deep: voices 2-8
	//p->addModel(modelMonsoonDeepStraitsSandsWest);  // Deep: voices 9-16
	// Visual editor expanders
	// Mono visual RETIRED (sands-consolidation Step 6): subsumed by Macro (7 lanes + mono mode).
	// Source moved to src/deprecated/; modelMonsoonSandsVisualExpander is now nullptr (see stub),
	// so cachedSandsVisualExpander detection is an inert no-op. Slug removed from plugin.json.
	//p->addModel(modelMonsoonSandsVisualExpander);   // Mono visual DNA editor
	p->addModel(modelStraitsEastSandsVisual);       // East visual DNA editor (tabbed)
	// RETIRED: West visual editor merged into East (15-voice). Source kept, not registered.
	p->addModel(modelStraitsSandsMacroVisual);      // Macro visual DNA editor
	p->addModel(modelIntertropical);                // Intertropical scene sequencer
}
