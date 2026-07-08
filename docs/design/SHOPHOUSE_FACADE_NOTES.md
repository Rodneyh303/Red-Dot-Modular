# Shophouse facade — resume notes (the fun part)

Status: **functional layer DONE and confirmed on hardware. Facade redesign DECIDED, not
built.** Paused to write the dataflow refactor plan (`DATAFLOW_DISCIPLINE_PLAN.md`). This note
captures exactly where to pick up so no context is lost.

Companion: `SHOPHOUSE_SPEC.md` (the scale-list mechanism — the *what/why*). This note is the
*panel-art + active-scale-indicator* resume point.

---

## 1. What's working now (functional — verified in Rack on Rodney's build)

All confirmed live, not just compiled:

- **4 scale-list fronts**, each = one `(scale, root)` slot. Per-front scale knob + 12
  clickable piano-keyboard shutters (click a shutter = set that front's root — the display IS
  the control, no menu).
- **Whole-shutter louvred fill**: each shutter lights the full key (teal in-scale / Singapore
  red root / dark out-of-scale) with horizontal louvre slats + frame — reads as a slatted
  shutter, not a dot. (`ShutterBank::draw`, rects mirrored from panel geometry.)
- **Live scale-name readout** per front on the name band ("C  Dorian" etc.) — `ScaleNameLabel`,
  font loaded *inside* `draw()` with `uiFont` fallback (ctor-load returned null → the two
  earlier blank-name failures).
- **Index CV attenuverter** (`INDEX_CV_ATT_PARAM`, −1..1) scaling the list-index CV.
- **Conservation toggle** (guide vs enforce → `lockScaleNotes`).
- **CV modulation between fronts WORKS** (this was the long hunt): unipolar 0..10V sweeps
  fronts 0..3, committed at the phrase boundary, active scale changes, Monsoon faders re-dim.
  Confirmed by log: `WRAP cv=9.35V idx=3 -> COMMIT active 0->3`.

### The chain of fixes that got CV modulation working (for context if it regresses)
1. `updateScaleMask()` now called on Shophouse commit (was only on patch-load / menu).
2. Active scale applied **every process frame**, not only on front-switch commit (covers
   startup + editing the active front's knob).
3. Sync + apply hoisted **out of `if(shouldExecute)`** so it runs when stopped / between edges.
4. **The real blocker:** `engine.lastStepResult.wrapped` was always false — `executeStep`
   assigned `lastStepResult` *before* `executeModeA/B` set `wrapped` on the returned copy. Now
   re-synced. This is why the boundary sampler never fired. (See `DATAFLOW_DISCIPLINE_PLAN.md`
   bug #6 — same class.)

---

## 2. The facade problem

Current panel = a functional grid of shuttered windows with a small pediment. Rodney's verdict:
"nothing like a shop house." Reference: a framed **Singapore Peranakan shophouse poster**
(eck&art design studio, on his wall — photo in chat 2026-07-05). Emerald Hill Rd, where he
lived.

### What makes the poster read as a shophouse (the vocabulary to build)
Top → bottom of the poster:
- **Hipped tiled roof** (trapezoidal, brown) with a small dormer/ventilator box on top — NOT a
  triangular pediment (my first attempt was wrong).
- **Cornice string-course** under the roof with **fern/foliage plaster reliefs** + rosettes.
- **Upper floor: arched windows** with **louvred timber shutters** (horizontal slats — the
  shutter look), framed by **pilasters**, flanked/separated by **Peranakan majolica tile
  panels** (square floral tiles — *the* signature element).
- **String course** dividing the two floors.
- **Ground floor / five-foot-way**: recessed entry, **pintu pagar** (low swing gate) centre,
  barred windows either side, hanging lanterns, tile panels at the base.
- **Palette**: dusty mauve-pink facade, sage/grey-blue shutters + trim, white pilasters +
  window frames, brown roof.

---

## 3. DECISIONS (settled with Rodney — build to these)

**Structure — TWO shophouses side by side**, each 2–3 storeys, **2 scales per house**:
- House A (left) = scales/fronts **0, 1** (window per floor).
- House B (right) = scales/fronts **2, 3**.
- **Five-foot-way** runs along the base (both houses), housing CV in / attenuverter /
  Conservation.
- **Widen the panel** if necessary to fit two houses cleanly. (Currently 20HP.)

**Elements to build** (Rodney's pick, in priority): pilasters framing windows · louvred
shutters (already have the fill) · **Peranakan tile panels — take inspiration from the
Interchange panel's approach** (read `MonsoonInterchangeExpander` panel + its gen script first
for consistent tile motif) · five-foot-way.

**Colour**: Peranakan pastels but tuned to the dot.modular palette; **dark shutters** so the
lit scale notes stay legible. (Facade washes already muted in `FACADE` dict of
`gen_shophouse.py` — teal/ochre/rose/slate.)

**NEW REQUIREMENT — active-scale indicator.** Now that the scale genuinely changes under
modulation, there must be a **visual cue showing which of the 4 fronts is currently live**
(the committed `active_`). Rodney: "we need a visual way to show which scale is the current
one." Design open — candidates: the active window's shutters get a brighter/framed glow; a lit
lantern over the active house; the active front's name band highlighted. **Decide this with
Rodney before building.**

---

## 4. Constraints (nanosvg — hard)

Panels are nanosvg-parsed: **solid fills + strokes + closed arc paths only.** NO gradients,
`<mask>`, `<text>`, `url()`. Elliptical arcs (`A` in paths) ARE supported (precedent:
`UltraProV2_main_panel.svg`, and the current five-foot-way arches). So: stylised vector
illustration, not photo-realism — the *iconic silhouette* (roof, pilasters, arched louvred
windows, tile panels, five-foot-way) is achievable in layered solid paths.

**Every functional marker position must be preserved** so widget bindings/hit-tests hold:
`shutter_<f>_<0..11>` (48), `name_band_<f>` (4), `param_scale_<f>` (4), `param_indexcvatt`,
`param_conservation`, `input_indexcv`, `light_connect`. The art frames *around* these; it must
not move or obscure them. Live shutter fill is drawn by the widget over the panel recess, so
panel shutter rects are just fallback/preview.

Live text (scale names) = `nvgText` in the widget draw, font loaded in `draw()` with `uiFont`
fallback (never ctor-time — it returns null).

---

## 5. Resume checklist (next session)

1. Read the **Interchange panel + its gen script** to lift the Peranakan-tile motif approach.
2. Agree the **active-scale indicator** design with Rodney (§3).
3. Rewrite `panel_src/gen_shophouse.py` as **two houses side by side** (fronts 0,1 | 2,3),
   two-storey each, five-foot-way base, tile panels, louvred arched windows, pilasters, hipped
   roof. Widen HP if needed. Keep every marker id/position.
4. Add the **active-front visual** in `MonsoonShophouseExpanderWidget` (reads
   `shop->list.active()` — already exposed).
5. Verify: 48 shutter markers + all controls present, valid XML both themes, no
   `<text>`/gradient/mask/url, arcs closed. Screenshot-iterate against the poster with Rodney
   (this kind of art always takes 2–3 render passes — same as the Lantern keybed).

---

## 6. Open bug parked alongside (not facade, but Shophouse-adjacent)

Persistent `StrandLedger CONFLICT MACRO then EAST` every block (see
`DATAFLOW_DISCIPLINE_PLAN.md` Step 2). Non-fatal (last-writer-wins) but real. Not a facade
blocker; fix on the dataflow track.

---

## 7. Facade progress + refinement backlog (2026-07 build session)

**DONE (on branch `feat/shophouse-facade`, off `integration/probmod-spread-macro`):**
- Widget **decoupled** from hardcoded shutter geometry — reads shutter RECTS straight from the
  panel `<rect>` markers via new `ShapeQuery::boundsOf()`. Panel is the single geometry source;
  layout can change freely. (Removed the whole hardcoded-rect block from the widget.)
- **Two-house facade**, 26HP (was 20HP/4-stack). House A (fronts 0,1) left, House B (2,3) right,
  two storeys each, one arched louvred window per floor. Hipped roof + dormer, cornice reliefs,
  string course, pilasters, five-foot-way colonnade.
- **Poster-corrected** (against Rodney's eck&art reference): killed the big dominant semicircle
  tympanums (my invention) → **shallow segmental arch caps**; shutters shifted to teal-grey;
  roof flattened/widened to a low overhanging hipped roof with dark-window dormer; facades to
  Peranakan mauve-pink. **Interchange majolica tile bands kept** (the tie-in Rodney liked).
- **Active-scale lantern indicator** (`LanternWidget`): lit red lantern over the committed-active
  front (`list.active()`), dark over the rest — resolves house + floor, hops on CV commit.

**STILL NEEDS ART REFINEMENT (Rodney's eye + agreed items — do in a later render pass):**
1. **Five-foot-way** is thin — deepen it, make the colonnade arches proper, seat the controls on
   a base (currently they float). Add pintu-pagar gate + barred side windows + base floral tile
   panels per the poster ground floor.
2. **Roof detailing** — still reads a bit flat; poster roof is clean but has crisp eaves shadow +
   the dark ventilator box. Tune tile courses / eaves.
3. **Colour polish** — dark-theme facades still a touch muddy; make majolica tiles pop a little
   more against the wash. Keep shutters legible (widget overpaints them live).
4. **Shutters take a little colour** (Rodney) — the resting/preview shutter fill could carry a
   hint of the teal rather than pure grey (live widget fill already does in-scale teal / root red
   / out dark).
5. Windows/arches are now right (shallow caps) — do NOT reintroduce big arches.

All art is container-verifiable via cairosvg; iterate against the poster. Widget code
(shutter reads, lantern) is build-gated — needs Rack build-verify.

## 8. Live scale-note indication (the piano-keyboard shutters lighting per active mask)

The shutters ARE the live scale display: for each front, the widget lights each of the 12
key-shutters by that front's (scale, root) mask — in-scale = teal, root = Singapore red, out =
dark. This is `ShutterBank::draw` reading `params[SCALE_PARAM_0+f]` / `ROOT_PARAM_0+f`. For the
ACTIVE front specifically, the "current playing scale" reads live off the same mask; the lantern
marks WHICH front is active. (If a stronger "live note played" pulse is wanted later — e.g. the
currently-sounding degree flashing — that would read the engine's live step, a separate hook.)

