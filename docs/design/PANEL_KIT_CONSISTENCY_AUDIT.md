# SVG panel kit consistency — audit TODO (Rodney)

STATUS: TODO, not yet done. Raised after CC found Sands Mono and Macro drawing the CONNECT MARK by
hardcoded coordinates while other modules bind it from the panel. The survey below shows the problem
is suite-wide, not a Sands quirk.

## Why this matters (it has cost three sessions already)
Every hardcoded `mm2px` placement or label coordinate in a widget DUPLICATES geometry the panel
generator already owns. Two sources of truth for one layout means:
- re-running a generator silently undoes a C++ fix (and vice versa);
- a widening or lane insertion must be applied twice, correctly, or things drift;
- the drift is invisible until someone LOOKS at the panel.
Worked example: the Macro MIX IN labels drifted because `gen_macro_mono.py` used `GROUP_W = ED_W/4`
while `StraitsSandsMacroVisual.cpp` used `ED_W/5` — progressive misalignment, labels landing on the
neighbouring group. Same class as the Sands lane-index bugs and the Mono spread no-op: ONE QUANTITY
DEFINED IN TWO PLACES.

## Survey (kit calls vs hardcoded mm2px, per widget)
| module | kit | mm2px | note |
|---|---|---|---|
| MonsoonWidget | 76 | 21 | most migrated, still mixed |
| MonsoonRafflesExpander | 23 | 2 | |
| MonsoonStraitsExpander | 14 | 3 | |
| MicroTuning (Colonnades/Duo) | 10 | 3 | |
| Keppel | 9 | 8 | half; being widened for MPE — migrate DURING that work |
| MonsoonCausewayPolyExpander | 9 | 1 | |
| MonsoonShophouseExpander | 8 | 6 | |
| MonsoonSandsVisualExpander (Mono) | 7 | 5 | connect mark hardcoded (the original finding) |
| MonsoonShophouseMicro | 7 | 2 | |
| StraitsEastSandsVisual | 6 | 10 | |
| MonsoonChangiExpander | 4 | 1 | |
| Sikit | 4 | 2 | |
| StraitsSandsMacroVisual | 4 | 13 | connect mark hardcoded (the original finding) |
| Intertropical | 3 | 12 | |
| MonsoonChangiT2Expander | 3 | 1 | |
| MonsoonJunctionExpander | 3 | 2 | |
| MonsoonChangiT3Expander | 2 | 1 | |
| **Lantern** | **0** | 6 | NO kit use at all |
| **MonsoonInterchangeExpander** | **0** | 13 | NO kit use at all |

Every widget is mixed. Two use no kit at all.

## The rule (already in panel_src/README.md, apply it everywhere)
1. The generator emits an anchor for EVERY control: `param_* / input_* / output_* / light_*`,
   visible (never `display:none` — nanosvg drops hidden shapes, which blanked Monsoon's controls once).
2. The widget binds by name and has ZERO `mm2px` placements.
3. Labels and framing in `draw()` derive from anchors — `centerOf(findNamed(...))` plus a fixed dy —
   never recomputed geometry. Moving a jack in the generator then moves its label automatically.
4. Run the anchor/bind audit in `test/run_all.sh`: every anchor bound, every bind resolves. That turns
   "a control or label is misplaced" into a BUILD FAILURE instead of something Rodney finds by looking.
   It currently covers 3 migrated modules; it should cover all of them.

## Suggested order
1. **Keppel** — migrate as part of the MPE widening (the panel is being regenerated anyway, so it is
   nearly free now and pure cost later).
2. **Lantern and Interchange** — no kit at all; greenfield, no half-state to reconcile.
3. **Macro (13) / Intertropical (12) / East (10)** — worst mm2px counts, and Macro/East already have
   a history of drift.
4. **MonsoonWidget (21)** — biggest, but also the most migrated; finish it last so the pattern is
   settled first.
5. The long tail (1-6 each) — mechanical.

Per module: check a generator exists and is the ACTIVE one (`panel_src/README.md` table), add any
missing anchors, replace placements with binds, re-point labels, add the module to the audit, render
and diff with `panel_src/panel_diff.py` (nanosvg renderer) to prove nothing moved.
