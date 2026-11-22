# Fold container architecture — reusable base for the group panels

Goal (Rodney): one reusable class/template so a new fold container (Monsoon group, Sands Macro,
Sands East, Straits, Change Alley) is cheap to create, with little or no rework of the existing
panel/expander classes — both the plugin (Module) half and the widget (ModuleWidget) half.

## Why this is feasible at all — the de-param store law (the enabling precondition)
The existing panels' CONTROLS are already store-backed, not Rack params: a knob reads/writes the
host Monsoon's `EditorState` via `placeStoreKnob<Monsoon,…>` + `lookupMonsoon(module)` (the
topology-epoch lookup, `ui/StoreBound.hpp` / `ui/ModuleLookup.hpp`). SandsFold already re-hosts the
98 Sends knobs this way. Because the DATA lives in Monsoon, a control can be drawn by ANY widget
pointing at the same store — no persistence migration, no param-id juggling. **If these were Rack
params on each panel's own Module, this whole scheme would be impossible** (you can't move a Rack
param to another module without breaking patch restore). The de-param law is what makes the
container a pure re-hosting of existing store-bound widgets.

JACKS are the exception — a port is a real Rack port owned by exactly one Module. See "Ports".

## The two reusable bases

### Plugin side — `redDot::fold::ContainerModule : rack::Module`
Holds only container state, NO band data (that stays in Monsoon's EditorState):
- `FoldState`: the ORDERED enabled-band set + per-jack-panel edge/side + viewport scroll/visible
  (serialized; the saved state is the band SET, width is recomputed — see FoldBandSetCore).
- `config()` declares the FROZEN UNION PORT ENUM — every band's ports for every band, up front
  (Rack runs config() once; a band popped out just reveals already-declared port widgets). Each band
  contributes a `declarePorts(Module*, int baseId)` that the container calls with the band's slice of
  the union enum.
- `Monsoon* host()` via `lookupMonsoon` (the store reach), same as SandsFold.
- No params, no stores of its own (the de-param law).
Concrete containers (`MonsoonGroup`, `SandsMacroFold`, …) are thin: they list their band set and
their port-enum slices; the base does the rest. A template `ContainerModule<BandList>` or a plain
base + a `bands()` override both work; prefer the base + virtual `bands()` (Rack Models need a
concrete type per slug anyway).

### Widget side — `redDot::fold::ContainerWidget : rack::ModuleWidget`
The generic composition engine — this is where the reuse pays off:
- `rebuildLayout()`: width = `FoldBandSetCore::widthHpForBands(base, enabledBands)`; order =
  `FoldBandSetCore::layoutOrder(...)` (jack panels edge-pinned); rects =
  `FoldBandLayoutCore::layoutBands(totalPx, specs)`; then for each band call
  `band.place(this, host, rect.x0Px)`.
- `draw()`: ONE procedural background fill (SandsFold's pattern) + per-band `band.draw(args, rect)` +
  `FoldBandLayoutCore::dividerX` rules + `StraitsSeamFiller` at the edges.
- resize / neighbour-push (`setModulePosForce`) / exact-restore undo: the SandsFold spike's logic,
  generalised into the base (already Rack-free-cored in FoldPanelWidthCore).
- pop-in/out: toggles the band set → `rebuildLayout`; the cabled-jack guard is
  `FoldBandSetCore::removalNeedsGuard(isJackPanel, hasCable)` before a pop-in.
- `FoldViewportWidget : ContainerWidget` — the Straits variant; the knob bands become ONE viewport
  band driven by `FoldViewportCore` (show N, scroll the rest). Only this subclass carries scroll.

### The Band adapter — the one small per-panel surface
```
struct FoldBand {               // one existing panel, re-hostable at an x-origin
    int       id;
    const char* title;          // the subpanel name (titled bands)
    float     contentExtentPx;  // feeds FoldBandSetCore (from the panel's gen_*.py geometry)
    bool      isJackPanel;      // edge-pin + removal guard
    BandEdge  edge;             // Left (jacks pop out left) / Interior / Right
    int       portBase, inCount, outCount;                      // its slice of the union enum
    void declarePorts(rack::Module* m) const;                   // config()-time
    void place(rack::ModuleWidget* host, Monsoon* store, float x0) const;  // knobs + port widgets at mm+x0
    void draw(const DrawArgs& args, float x0) const;            // captions / labels / lane art
};
```
A virtual interface or a struct-of-fn-pointers both fit; a virtual `IFoldBand` is the Rack-idiomatic
choice. Each existing panel provides ONE concrete band.

## The per-panel refactor recipe (mechanical, golden-safe, little rework)
Existing panel widgets embed placement + draw in their ModuleWidget constructor. To share them with
a container WITHOUT a rewrite, extract (don't rewrite) three position-parameterised free functions
per panel, then call them from BOTH sites:
1. `placeXBand(host, Monsoon* store, float x0)` — the knob/port placement loop, `+ x0` on every mm
   station (SandsFold already does this inline for Sends — lift it to a shared function).
2. `drawXBand(const DrawArgs&, float x0)` — the captions/labels/lane art, `+ x0`.
3. `declareXPorts(Module*, int base)` — the port declaration, parameterised by base id.
Then: the STANDALONE panel widget calls them at `x0 = 0, base = its own enum`; the container's band
adapter calls them at `x0 = band origin, base = the union slice`. Behaviour is identical at x0=0, so
the standalone module's goldens/feel are unchanged — the extraction is a pure move. No existing class
is rewritten; each gains a 2–3 function extraction and delegates to it.

## Ports — the one real coupling (not solvable by re-hosting alone)
A port is owned by one Module. Today the engine reads an expander's jacks through typed cached
pointers in `MonsoonExpanderManager` (`cachedRafflesExpander`, `cachedCausewayPolyExpander`, …). When
a band's jacks move onto the CONTAINER module, the engine's read path must find them there. Two options:
- (A) the container implements the same port-read interface the manager expects, and the manager
  caches the container (add a `cachedFoldContainer` + teach each reader to prefer it). Cleanest.
- (B) a thin shim: the container exposes `getBandInput(bandId, localId)` and the readers call through
  it. Less invasive but spreads the indirection.
Recommend (A): one new cached pointer + each affected reader gains a "container present? read its port
for this lane" branch — mirrors the existing expander-presence checks. This is the ONLY place the
engine changes; everything else is widget/store composition. Freeze the union port enum before any
of this (GROUP_PANEL_INVENTORY.md is the input).

## How the existing cores slot in (all built + tested)
- `FoldPanelWidthCore` — clamp/snap/min-content, neighbour shift, exact-restore undo (the resize base).
- `FoldBandSetCore` — width from the enabled set, edge-pinned order, removal guard.
- `FoldBandLayoutCore` — exact band tiling, integer dividers, title-fit (the draw/place layout).
- `FoldViewportCore` — the Straits show-N-scroll viewport.
- `FoldBandLayoutCore` + the band adapter are the two the ContainerWidget leans on every frame.

## Build order (incremental, each shippable)
1. Generalise SandsFold → `ContainerWidget`/`ContainerModule` base; re-express the Sends-only fold as
   one band through the base (behaviour-identical — the regression pin).
2. Add the band adapter + extract `placeSendsBand`/`drawSendsBand` (the first panel; proves the recipe).
3. Freeze the union port enum (per group) → add the Distribution + Macro bands (ports: option A).
4. `FoldViewportWidget` for the Straits group.
5. Each further panel: the 3-function extraction + one band adapter.

## Non-goals / cautions
- Rack cannot nest one Module's live ModuleWidget inside another's — hence the function-extraction
  recipe rather than embedding existing widgets.
- Keep each band's draw/place Rack-thin; the geometry numbers stay in the gen_*.py-derived constants.
- The container owns no store; a band that needs new storage (e.g. the 56 Distribution depth knobs)
  adds it to Monsoon's EditorState, not the container.
