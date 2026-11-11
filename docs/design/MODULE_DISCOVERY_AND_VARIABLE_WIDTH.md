# MODULE DISCOVERY BY ID AND VARIABLE WIDTH (design note, from the Tessera reference; reference only, GPL-3.0)

Source of the pattern: Tessera's Roll module (res/tessera-main.zip on feat/phase-duals-gaps). We learn the design; we do
not copy code.

## WHAT TESSERA DOES
- The editor is part of ONE module. Output modules find their host by ID: findTessera = dynamic_cast of
  APP->engine->getModule(moduleId) (src/module/CommandAction.cpp).
- Width is saved state (editor().panelHp). applyPanelWidth sets box.size.x = hp * RACK_GRID_WIDTH and calls
  setModulePosForce so neighbours move out of the way. Width changes are on the undo stack.

## APPLICABILITY (per module)
- SANDS FAMILY, popped-out subpanels (distribution, sends, modulation): ID discovery fits. A subpanel can sit anywhere
  and finds its host or store by ID. Replaces the adjacency walk that has been breaking the chain.
- SANDS PROBABILITY VISUAL: variable HP fits (the bar grid is the wide part; the user picks the width).
- CHANGE ALLEY: wide module, variable HP fits.
- STRAITS: subpanels can pop out and find their host by ID; variable HP fits its grids.

## NOTES ON OUR CONSTRAINTS (Rodney)
- All our cross-module data is ZERO-LATENCY POINTER WORK. We never use expander messages, so there is no adjacency
  requirement for data. ID discovery is only a lookup for which module to point at.
- We do NOT look up modules on the audio thread. Lookups happen in the UI step and the audio path reads the cached
  pointer or the store. This constraint is already met; keep it.
- THE TOPOLOGY EPOCH MUST BUMP ON ANY MODULE ADD OR REMOVE, not only on an adjacency change. A popped-out subpanel can
  appear anywhere, so an adjacency-only epoch leaves a stale null. This is the memo bug found earlier, and it still
  applies to ID discovery.
- IDS CHANGE ON PASTE AND DUPLICATE. Re-resolve when an ID changes.

## DEFERRED
- VARIABLE WIDTH is significant work: the SVG panel art would need a procedural background, with the grid scaled to
  the width. Not scheduled. Revisit only after the discovery change is proven.

## OPEN
- Whether to pop out the Sands subpanels (distribution, sends, modulation) as separate modules, or keep them as expanders.
- Whether the probability visual gets a width setting (deferred, see above).
