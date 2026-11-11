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

## CAVEATS (must hold for us)
1. AUDIO-RATE DATA STILL NEEDS THE EXPANDER LINK. Rack expander messages need adjacency. ID discovery is for control and
   UI, and for store reads.
2. NEVER LOOK UP MODULES ON THE AUDIO THREAD. Resolve by ID in the UI step, cache the pointer, and let process() read the
   cached pointer or the store only. (Tessera resolves in the UI and uses parameter handles on the audio side.)
3. THE TOPOLOGY EPOCH MUST BUMP ON ANY MODULE ADD OR REMOVE, not only on an adjacency change. A pop-out subpanel can appear
   anywhere, so an adjacency-only epoch leaves a stale null. This is the memo bug found earlier.
4. VARIABLE WIDTH BREAKS SVG PANEL ART. Our panels are SVG. A procedural width needs a drawn background, with knobs fixed
   to the grid and the grid scaled to the width.
5. IDS CHANGE ON PASTE AND DUPLICATE. Re-resolve when the ID changes; keep names unambiguous (Tessera renames duplicates).

## OPEN
- Whether to pop out the Sands subpanels (distribution, sends, modulation) as separate modules, or keep them as expanders.
- Whether the probability visual gets a width setting, and its SVG-to-procedural approach.
