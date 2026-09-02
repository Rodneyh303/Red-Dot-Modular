# CA panel: fitting the 3rd stream (q-mix) -- layout options + decision

Problem: q-mix adds a THIRD stream, so CA's per-stream control rows go from 2 (melody, rhythm) to 3
(melody, rhythm, q-mix). Each stream has jack+knob rows for collapse/rotate/reflect/scatter, mirrored
L(Intra)/R(Inter), jacks AROUND THE EDGES. Three streams x 4 ops -> ~12 rows. ~8 rows is the Rack norm; 12
is tight. Constraints to satisfy TOGETHER: (a) 12 rows of controls, (b) jacks at panel EDGES (the nice
ergonomic -- cables patch at the boundary), (c) comfortable size. Eurorack HEIGHT is fixed (128.5mm/3U),
so row count is height-bound -- can't grow vertically.

## DECISION: try smaller jacks/knobs FIRST, see how it lands (Rodney)
Simplest, lowest-disruption: keep EVERYTHING liked (edge-jacks, Intra/Inter mirror, per-stream rows,
single panel), just shrink jack/knob size + row pitch to fit 12 rows. Try it, RENDER it, eyeball the
ergonomics, THEN decide. Defer the structural options until there's actual evidence (a rendered panel),
don't reason in the abstract.
Well-founded: only need 12 rows; some modules fit 16 -> asking the shrink to do LESS than proven possible.
12-at-slightly-smaller is inside proven territory, good chance it just works, making the fancier options
moot.

### First step (at the machine)
Layout is owned by the generator script (gen_change_alley_v2.py; geometry "MUST MATCH" it). So: reduce
jack/knob component sizes + CTRL_ROW_H (row pitch) in the generator, REGENERATE, eyeball the 12-row result.
The container can't build the Rack SDK, but PANEL GENERATION is Python -- may run standalone to preview the
SVG without a full plugin build. Check if the generator runs standalone -> iterate on panel look without
building.

## Fallback ladder (only if the shrink doesn't land)
Tabbing is RULED OUT: it breaks edge-jacks (a tab is an inset panel region with NO panel edge -> jacks end
up mid-panel, losing the ergonomic). The options that PRESERVE edge-jacks:
1. FOUR EDGES: currently edges = left(Intra) + right(Inter) = the two VERTICAL edges. Use the TOP + BOTTOM
   horizontal edges too -- e.g. the q-mix stream's jacks along top/bottom, melody/rhythm on L/R. Keeps
   jacks on edges (all four), relieves row pressure, stays single-panel. Risk: jacks on all four sides can
   read busy.
2. EDGELESS CA EXPANDER: put the q-mix stream's 4 op-rows on a seamless expander. Its jacks sit on THAT
   panel's edges; CA's melody/rhythm jacks on CA's edges; reads as one continuous surface. Preserves (a)
   12 rows, (b) all jacks on edges, (c) comfortable size -- by using TWO panels' edges. This is WHY it
   succeeds where tabbing fails: a tab has no edge, an expander HAS edges. Cost: two panels wide (fine
   pre-release, panel growing anyway).

Preferred fallback = edgeless expander (satisfies all three constraints cleanly); four-edges if single-
panel matters and the busy look is acceptable.

## Why height is the real ceiling
Row count is bounded by panel HEIGHT with edge-jacks (128.5mm fixed). Widening (more HP) does NOT fix a
row-COUNT problem (adds horizontal room, rows are vertical). So the honest fixes are: shrink (fit more rows
in the fixed height), OR add panel area via an expander (more edges), OR use the top/bottom edges. Not
widening.

Cross-ref: MonsoonChangeAlleyV2.hpp:360-395 (geometry constants -- CTRL_ROW_H, jack/knob placement, the
things to shrink), gen_change_alley_v2.py (the authoritative generator to edit + regenerate), the CCA
scatter-grid sections in RANDOM_VS_INPUT_MODULE_CONCEPT (the 3rd stream = q-mix = the +rows), ROAD_TO_
RELEASE (pre-release: panel changes are free).

## The two CA poly mod inputs (GRAIN_POLY_IN, STEP_POLY_IN): don't scale to q-mix -> cut or move (Rodney)
CA has two poly modulation inputs (bottom-right under the last reflect row): GRAIN_POLY_IN (16ch -> 16
grain knobs, mono=all) + STEP_POLY_IN (same for step/leader). Per-voice CV mod of grain + step, mono-
normalled (one LFO modulates all 16). Designed for the 2-stream world (melody, rhythm).

### Key: they DON'T SCALE to the 3rd stream (q-mix)
Keeping them is NOT "leave as-is" -- they'd need REORGANIZING to cover 3 streams (grain/step/q-mix poly
mod), which means MORE inputs / a redesigned scheme = MORE panel pressure on the panel already being
fought, PLUS the reorg work. So keeping-on-CA is negative on BOTH axes that matter now: adds panel
pressure AND adds work -- it makes the EXACT problem being solved (fit the 3rd stream) WORSE.

### So status-quo is NOT an option; the choice collapses to two
- The poly mod inputs can't stay on CA regardless (don't scale, worsen the crunch).
- CUT entirely: lose the (partial, mostly-mono-normalled) grain/step CV mod; reclaim 2 EDGE-JACKS (exactly
  the edge real-estate the 3rd stream needs -- they sit bottom-right on the edge being freed); zero further
  work.
- MOVE + SCALE on CAUSEWAY: if per-voice grain/step(/q-mix) CV mod is worth keeping, it must move off CA
  anyway to scale (the scaling problem IS a panel problem; Causeway is a different panel with room). Reorg-
  to-3-streams and move-to-Causeway are the SAME motion: keep it -> it lives on the CV expander and scales
  there, CA's edge freed. Idiomatic (mod CV on the CV expander).
Keeping-on-CA is OFF THE TABLE (doesn't scale + worsens the panel problem).

### Steer
Default CUT (Rodney leaning that way; status-quo isn't available; panel pressure favours it; the lost
capability is modest -- partial, mono-normalled grain/step CV mod). MOVE+SCALE to Causeway ONLY IF per-
voice grain/step CV modulation is a gesture worth preserving. Either way the 2 edge-jacks are reclaimed ->
directly eases the 12-row fit. This makes the decision EASIER: the status quo doesn't survive the 3rd
stream, so it's a clean binary (cut vs relocate-and-scale), and the panel crunch points at cut unless the
feature earns relocation.

Cross-ref: MonsoonChangeAlleyV2.hpp:139/165/525-535 (GRAIN_POLY_IN/STEP_POLY_IN -- the inputs, their
mono-normal mod, their bottom-right edge placement), the shrink-first decision above (cutting these 2
jacks eases the 12-row fit directly), Causeway (the CV-expander home if kept+scaled), ROAD_TO_RELEASE
(pre-release: cutting/moving inputs is free)." 

## UPDATE (Rodney): DON'T cut the poly mod inputs yet -- defer pending the panel reorg
Correction to the cut-vs-move framing: don't cut yet. Growing the panel for the q-mix subpanels (which the
3-stream problem forces anyway) CREATES room -- and in that larger, reorganized panel there might be space
to KEEP + properly reorganize the poly mod inputs (scaled to 3 streams). So the cut decision is PREMATURE:
it's downstream of a panel-growth-and-reorg not yet done.

### Why deferring is right (don't decide against a constraint you're about to change)
The prior cut-vs-move framing treated the panel as FIXED ("given no room, cut or relocate?"). But the
panel ISN'T fixed -- you're already going to grow/reorganize it for q-mix, and that reorg is a NEW CONTEXT
in which the poly-mod question should be re-asked. Cutting now, against the CURRENT cramped layout, throws
away an option the REORGANIZED layout might restore for free. Don't spend the decision against a constraint
about to change.

### Dependency ordering
1. FIRST: grow the panel + reorganize for the q-mix subpanels (needed regardless).
2. THEN: in the new layout, SEE whether there's room to keep the poly mod inputs (reorganized to 3
   streams).
3. ONLY THEN: decide keep-reorganized / move-to-Causeway / cut -- with actual knowledge of the space, not
   a guess against the old cramped panel.
Same empiricism as the shrink-first decision: reorganize, render, LOOK, then decide -- don't decide against
an imagined constraint.

### Deferral WITH A TRIGGER (so it's not forgotten)
This is a deferral, not an open-ended maybe. TRIGGER: once the panel is grown/reorganized for q-mix,
REVISIT the poly-mod inputs. Recorded as a PENDING DECISION tied to the reorg -- must be resolved then, not
dropped. The one bad outcome to avoid: the inputs lingering half-scaled (working for 2 streams, ignored
for the 3rd). So: defer, but flag it as a decision the reorg MUST resolve.

Supersedes the "default CUT" steer above: status-quo-on-CA still doesn't survive as-is, BUT the resolution
(keep-reorganized / move / cut) is DEFERRED to after the panel reorg, not decided now. Not cut yet.

Cross-ref: the poly-mod-inputs section above (the cut-vs-move options -- now deferred pending reorg), the
shrink-first decision (same reorganize-render-look-then-decide empiricism), gen_change_alley_v2.py (the
reorg happens here; the poly-mod fate resolves once the q-mix subpanels are placed)." 

## PROPOSED REDESIGN (Rodney): lose L/R symmetry -> 6-column grid by component-type sections
Abandon the L=Intra / R=Inter mirror. Organise by 6 COLUMNS = (rhythm, melody, q-mix) x (intra, inter) =
3 streams x 2 sides. Stack in COMPONENT-TYPE sections down the panel:

- JACKS section: 6 columns x 10 rows (r/m/q x intra/inter columns; 10 op-rows).
- BUTTONS section: same 6 columns x 10 rows.
- KNOBS section: 6 columns x 6 rows. Row breakdown CONFIRMED by verb structure: Collapse (2 knobs) +
  Rotate (2) + Reflect (1) + Scatter (1) = 6 knob-rows x 6 columns = 36 knobs. Clean.
- Then the PIN MATRIX (16x16), and to its RIGHT the 3 CCA submatrices (2x2, 2x2, 3x3 = the self-reference /
  scatter-correlation grids). Then any MOD JACKS (the deferred poly-mod inputs).

### Why sound
- Folds Intra/Inter into COLUMNS (6 = 3x2) not PANEL HALVES -> reclaims the space the mirror wasted on a
  2-state distinction; the fundamental move that lets 3 streams fit. (The concrete form of "demote the
  mirror".)
- Groups by COMPONENT TYPE (jacks/buttons/knobs blocks): uniform grids, easy to scan + GENERATE, at the
  cost of per-operation locality (an op's jack/button/knob now in different sections). For a setup-then-
  play module, section-wise is a reasonable trade.
- Knob section (6x6) = the clean CONFIRMED anchor (Collapse2+Rotate2+Reflect1+Scatter1); build from it out.

### Flags (honest)
1. The "10" jack-rows / 10 button-rows needs its BREAKDOWN CONFIRMED. Code shows each verb has DOMAIN +
   CODOMAIN sides (dom/cod triggers; "4 domain + 4 codomain reverse buttons") -> the 10 is dom/cod-driven,
   but 4 verbs x dom/cod = 8, not 10 -> 2 more (forward/back on some verbs? an extra on collapse/rotate?).
   Rodney to confirm the exact 10; not fabricated here. Once confirmed the arithmetic is nailed.
2. ~60 jacks + ~60 buttons (6x10 each) = a LOT of components. Uniform grids make it TRACTABLE (no mirror
   waste) but 120 jacks+buttons + 36 knobs + 16x16 matrix + submatrices = a BIG panel, likely much wider
   than the current 48HP. Fine pre-release, but the redesign implies a substantially LARGER CA.
3. GENERATOR-OWNED: goes in gen_change_alley_v2.py as three uniform grids (6x10, 6x10, 6x6) + matrix +
   submatrices. Uniform grids are MUCH easier to generate than the old bespoke mirrored rows -> the reorg
   is also a GENERATOR SIMPLIFICATION (a real plus).

### Status
The chosen direction for fitting 3 streams: reorganise the whole panel, lose symmetry, 6-column component-
type sections. Supersedes "shrink-first / edgeless-expander / four-edges" as the primary plan (those were
for keeping the old mirrored layout; this redesigns instead). Confirm the "10" breakdown, then it's a
generator rewrite (three uniform grids + matrix + submatrices). Poly-mod inputs: still deferred -- see if
the new layout has room (per the prior deferral).

Cross-ref: MonsoonChangeAlleyV2.hpp:111 (VN = Collapse/Rotate/Reflect/Scatter, N_VERBS=4), :54-62 (dom/cod
per verb; 4 domain + 4 codomain reverse buttons = the dom/cod doubling behind the 10), the knob 6x6
arithmetic, the CCA submatrices section in RANDOM_VS_INPUT_MODULE_CONCEPT (2x2/2x2/3x3 to the matrix's
right), gen_change_alley_v2.py (the rewrite target: three uniform grids)." 

## The "10" CONFIRMED + knob mismatch corrected (Rodney)
Jack/button 10 rows = Collapse 2 + Rotate 2 + Reflect 2 + Scatter 4:
- The 2s = DOMAIN + CODOMAIN (every verb has a dom jack + a cod jack).
- Scatter's 4 = domain + codomain, x FORWARD + REVERSE (scatter is the only verb with a reverse).
2+2+2+4 = 10. Matches the code: "4 domain + 4 codomain REVERSE buttons"; signed scatter counters (fwd jack
= counter++, back jack = counter--). So scatter being the ONLY reversible verb is WHY it's 4 while the
others are 2 -- the reversibility (signed Philox) shows up PHYSICALLY as scatter's extra fwd/rev jacks.

### Corrects the knob arithmetic: knobs are a DIFFERENT distribution from jacks
Earlier "confirmed" knobs as Collapse2+Rotate2+Reflect1+Scatter1 = 6 -- but that's a DIFFERENT per-verb
shape from the jacks. Reconcile:
- Jacks/buttons: Collapse 2, Rotate 2, Reflect 2, Scatter 4 = 10 (dom/cod all; scatter +fwd/rev).
- Knobs:         Collapse 2, Rotate 2, Reflect 1, Scatter 1 = 6.
DIFFERENT distributions -- Reflect has 2 jacks but 1 knob; Scatter has 4 jacks but 1 knob. That's FINE
(jacks and knobs needn't parallel -- a verb can have more trigger-jacks than value-knobs), but it means the
jack section (6x10) and knob section (6x6) have GENUINELY DIFFERENT ROW STRUCTURES. The generator must lay
each block out INDEPENDENTLY -- do NOT share a row template between the jack block and the knob block
(assuming they align is the trap).

### Fully-pinned sections (x 6 columns each = r/m/q x intra/inter)
- JACKS: 10 rows (collapse2, rotate2, reflect2, scatter4) -> 60 jacks.
- BUTTONS: 10 rows (button twins of the jacks) -> 60 buttons.
- KNOBS: 6 rows (collapse2, rotate2, reflect1, scatter1) -> 36 knobs.
- + 16x16 matrix + 2x2/2x2/3x3 submatrices + mod jacks.
Sections have DIFFERENT vertical structures (10/10/6) -- laid out independently, aligned by COLUMN (the 6
columns consistent across sections) but NOT by row.

### Redeeming locality
The 6 columns ARE consistent across all three sections (r/m/q x intra/inter in every block), so a jack, its
button-twin, and its knob(s) for a given (stream, side) sit in the SAME COLUMN across sections -- different
row-heights, same column. So the component-type sectioning loses per-OP row-locality but keeps per-(stream,
side) COLUMN-locality. A redeeming feature of the section approach.

Supersedes the earlier "knobs 6x6, arithmetic confirmed" as if it paralleled the jacks -- it does NOT
parallel; jacks are 10 (collapse2/rotate2/reflect2/scatter4), knobs are 6 (collapse2/rotate2/reflect1/
scatter1), different distributions, independent block layouts, column-aligned.

Cross-ref: MonsoonChangeAlleyV2.hpp:59 ("4 domain + 4 codomain reverse buttons" = scatter's fwd/rev),
:64-67 (signed scatter counters, fwd=++/back=-- -> why scatter is the only 4), the redesign section above
(the three blocks -- now with correct 10/10/6 row structures and independent layout), gen_change_alley_v2
.py (lay the three blocks independently, column-aligned, not row-shared)." 

## PLAN (Rodney): 6-column reorganised layout AS the structure + try smaller jacks first within it
Combines the two threads (not a reversal): the 6-column reorganised layout (lose symmetry) is the
STRUCTURE; smaller jacks/knobs is the density LEVER within it; plan B is the further fallback if that
doesn't land.

Ladder:
1. PRIMARY STRUCTURE: 6-column reorganised layout (jacks 6x10, buttons 6x10, knobs 6x6, matrix + 2x2/2x2/
   3x3 submatrices + mod jacks, column-aligned, sections laid out independently). The structural fix that
   CAN fit 3 streams.
2. FIRST LEVER within it: smaller jacks/knobs + tight row pitch (CTRL_ROW_H) to make the 6x10 jack/button
   blocks fit. Render, eyeball, judge.
3. PLAN B (fallback if smaller-jacks doesn't land): edgeless expander (its own edges) or four-edges -- the
   "add panel area / more edges" options. [Confirm which Rodney means by 'plan B'; recorded as the
   escalation from small-jacks-in-the-6-column-layout.]

### Why more likely to succeed than small-jacks-on-the-old-mirror
The reorg ALREADY removed the mirror's wasted doubling (Intra/Inter -> columns, not panel halves). So the
small-jacks lever fits into an already spatially-efficient layout and only has to close a SMALLER remaining
gap. The two moves compound: reorg reclaims the big space (kills the mirror), small-jacks closes the rest.

### Honest note (jacks are the binding element)
The 6x10 jack + 6x10 button blocks = 60 jacks + 60 buttons; jacks (PJ301M) are the least-shrinkable
element (near their floor). So the small-jacks test STILL succeeds-or-fails on whether 10 rows of PJ301M
fit the column height at tight pitch. If it fails, it fails ON THE JACKS -> plan B (expander/four-edges =
MORE EDGES for jacks, not smaller jacks). "Try small jacks first" is right, with the clear-eyed
expectation that jacks are binding and plan B is about jack REAL-ESTATE.

Cross-ref: the redesign + '10'-confirmed sections above (the 6-column structure this fits jacks into), the
shrink-first decision (same reorganize-render-look empiricism), gen_change_alley_v2.py (the rewrite:
6-column sections, small components, tight pitch), the fallback ladder (four-edges / edgeless-expander =
plan B)." 
