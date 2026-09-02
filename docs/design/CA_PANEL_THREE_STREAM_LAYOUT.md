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
