# STRAITS POLY SUBGATE KNOBS — the planned 2nd-knob poly articulation (consolidated)

STATUS: PLANNED, not built. The containment module is merged (gate mode works on the DAG); this is the
remaining piece that completes PER-VOICE gate-mode articulation. Consolidates the spec scattered across
plans/subgate_dev_plan.md (the consumer-group model) + ARTICULATION_ENVELOPE_VS_CORRELATION.md ("the
planned 2nd knobs") into one home. Build after the decision-manager / with the resolver work.

## THE GAP IT FILLS
In gate mode TODAY: V1 sources the gate/subgate TIMING GRID (main+ratchet+ghost → one mono gate), and
poly voices make their OWN rest/variation decisions at those edges (clamped to the DAG containment via
decideMask). But poly voices do NOT yet have their own SUBGATE-DECISION response knobs — they inherit the
main/global thresholds. The 2nd knobs give per-voice control over the subgate articulation decisions
(ghost/ratchet rest, legato, pitch-hold, tie-across-rest).

## THE CONSUMER-GROUP MODEL (from subgate_dev_plan.md §consumer groups)
Subgate articulation decisions are CONSUMERS of a draw event. Key invariant: within a SINGLE draw event,
all consumers read the SAME sampled shaped value (no re-draw per consumer) — that is what makes SAME-EVENT
consumers correlatable; DIFFERENT-event consumers are independent. Because ghost/ratchet/main are DIFFERENT
events, they need SEPARATE THRESHOLDS to express different rates — so the consumer knobs EXPOSE control
over already-distinct draws (not fake independence).

## PHYSICAL HOME (Sands vs Straits)
- SANDS (full): keeps the SINGLE per-lane knob (main level); subgate behaviour INHERITS it unless assigned.
- STRAITS = the consumer-group HOME: optional groups of SMALL set-and-forget THRESHOLD knobs (shrink OK —
  assignment controls, not performance controls). Add the groups you want: ghost/ratchet rest, legato,
  tie-across-rest, pitch-hold.
- CAUSEWAY MOD COVERAGE REQUIRED: everything on Straits is Causeway-modulatable; a new Straits group WITHOUT
  its Causeway mod path = a modulation-layer seam. Add the Straits group AND its Causeway mod path TOGETHER.

## GLOBAL vs POLY scope (the assignment)
- GLOBAL subgate group: one threshold drives all voices' subgate decision of that kind (pattern-character;
  the common/cheap case).
- ASSIGNED POLY SUBGATE GROUP on Straits: opt-in, a FEW assignable groups (e.g. 3) — full per-voice control
  for the ghost/ratchet decisions the user prioritises. Costs Straits/Causeway space; capped + assignable
  (point the 3 at whatever you want poly). Decisions without an assigned group FALL BACK to global-subgate
  or main (graceful — no decision is ever uncontrolled).
- MELODY/OCTAVE have NO Straits poly knobs → the global-vs-poly choice does NOT apply to them; it applies to
  the Straits MAIN-GATE lanes that HAVE poly knobs (rest/accent/qmix/legato/variation).

## VARIATION IN GATE MODE (the specific 2nd-knob reuse)
- WITH subgates: Variation does PITCH-HOLD (hold vs re-draw pitch across the ratchet burst) — GLOBAL or
  POLY choice; POLY reuses the IDLE gate-mode Variation per-voice knobs (free — they're unused in gate mode
  otherwise).
- WITHOUT subgates: Variation is free for another gate-mode role (candidates: tie-across-rest,
  advance-playhead-on-tie-into-rest — open).
- GHOST pitch-hold cell: flagged to confirm — ghost is a gap/new-note, "hold" may be n/a there.

## RELATION TO CONTAINMENT + THE DECISION MANAGER
These knobs set the per-voice subgate DECISION thresholds; the containment DAG sets the per-voice gate
SUPPORT (clamp to ∩ parents). They compose: a poly voice makes its OWN subgate articulation (2nd knobs)
WITHIN its containment envelope (DAG clamp). Both are decide() inputs → the decision manager is the natural
home for wiring them (decide(prob, draw, lane, ownership, envelope, subgate-thresholds)). Build with the
decision-manager work; the containment consumption (gate clamp) and these thresholds are the two per-voice
inputs the unified decide() takes.

## DONE (when built) =
- Straits optional consumer-group threshold knobs (ghost/ratchet rest, legato, pitch-hold, tie-across-rest),
  each with its Causeway mod path.
- Global-vs-poly scope per group; a few (≈3) assignable poly subgate groups; graceful fallback (poly →
  global → main, never uncontrolled).
- Gate-mode Variation = pitch-hold (reusing idle gate-mode Variation per-voice knobs).
- Wired through the decision manager's per-voice decide() alongside the containment clamp.

## NOT THIS DOC
- The containment DAG (built, merged). The decision manager (consumes both; separate).
- MELODY/OCTAVE (no poly Straits knobs — out of scope).

────────────────────────────────────────────────────────────────────────────────
## TENTATIVE CONCLUSIONS — atom=tie, Sands distribution params, panel direction (Rodney)
These are WORKING CONCLUSIONS, not commitments. The subpanel idea especially needs proving before commit.

### ATOM=TIE on legato (Option 2 — the settled mechanism)
- Atom on the legato lane = TIE (hold pitch + sustain). Atom weight = tie probability FLOOR. Legato knob
  above the atom floor = legato-re-pitch. Below the floor, all legato is tie.
- No separate pitch-hold knob needed. No Variation repurposing needed in gate mode (frees Variation).
- Mode-independent (gate/clock/phase — it's the distribution).
- MO correlation on the legato atom → correlated ties across voices (unison sustain / counterpoint).
- Rest atom = rest-beats-legato (already established). Priority: rest atom > legato atom > continuous draws.
- Atom=tie was chosen over a 2nd Straits knob because: (a) no extra controls, (b) MO cross-voice
  correlation (the decisive advantage — knobs can't do it), (c) distribution-integrated / warp / copula
  composable, (d) mode-independent, (e) uses the already-established rail semantics.
- Trade-off accepted: atom weight couples with the legato distribution (changes the floor, legato knob
  "changes meaning"). The T/F/ATOM-DECIDES context menu surfaces this as a deliberate user choice.

### DISTRIBUTION PARAMS (including atom) belong on SANDS
- Atom weight is a DISTRIBUTION parameter → Sands owns the distribution → atom weight knobs belong on
  Sands, not Straits. This SAVES Straits space for per-voice subgate thresholds.
- Sands currently big; distribution is currently a popup. The popup may need to become a proper panel
  surface to give atom weight (and warp/tilt params) real knobs.

### NEW STRAITS PLAN (global base + optional per-voice)
- Straits base module: GLOBAL knobs (one set, all voices). Mirroring Monsoon, in line with Monsoon.
- Optional per-voice subpanel: per-voice controls for subgate thresholds + any per-voice overrides.
- Motivated by gate-mode subgates but applies to all modes.
- Straits space is LIMITED — stays in line with Monsoon, doesn't freelance.
- Tie does NOT need a Straits knob (atom on Sands handles it) — Straits spends its space on subgate
  consumer-group thresholds that genuinely need per-voice controls.

### PANEL ARCHITECTURE DIRECTION (TENTATIVE — needs proving)
A converging pattern is EMERGING across the ecosystem: modules grew deep, the answer may be base + optional
subpanels for depth. TENTATIVE (the subpanel idea needs development and proof before committing):
- Sands: base (commonly-touched) + subpanel(s) for distribution depth (atom weights, warp detail —
  currently buried in the popup → moving to a proper panel surface).
- Straits: base (global, Monsoon-mirrored) + subpanel (per-voice, subgate thresholds).
- CA: may eventually need refactoring too (4th colour/plane for the containment routing if that direction
  is pursued, but Cantonment is now its own module so this may be moot).
- Cantonment: already its own module (just merged + renamed).
ALTERNATIVES to subpanels NOT YET RULED OUT: expanded popups, context-menu-driven params, more expanders,
a redesigned single larger panel, or some combination. The subpanel is ONE candidate — the right one needs
proving (does the Straits subpanel prototype work well? is it discoverable? does it scale?). Don't commit
to subpanels-everywhere until the Straits subpanel is proven.

────────────────────────────────────────────────────────────────────────────────
## PANEL ALTERNATIVES — subpanel-expanders vs named standalone pairs (Rodney, with images)

Two approaches to the "module grew too deep for one panel" problem. BOTH are live candidates; the Straits
subpanel prototype + usage will prove which is right where.

### THE ADJACENCY PROBLEM (image 2 — the risk both share)
In VCV Rack, expanders can be physically separated by dragging other modules between them. When a Straits
lane expander gets separated from StraitsBase, it reads as a broken/orphaned fragment. The alternative
(named standalone pairs) degrades more gracefully — each module has its own identity and makes sense alone.

### APPROACH A: subpanel-expanders (current Straits lane design)
StraitsBase + StraitsLaneREST / StraitsLaneLEGATO / etc., chaining left. Per-lane, per-voice knobs.
ALREADY BUILT (StraitsLaneExpander.cpp). The 2x8 compaction (smaller controls, 2 columns of 8 voices)
is a panel-space improvement over the current 3-col layout.
- PRO: tightest panel space (just the controls, no own framing).
- PRO: reads as "one instrument" when adjacent.
- CON: reads as "broken fragment" when separated.
- BEST FOR: per-lane per-voice controls (where the lane-specific identity IS the subpanel identity).

### APPROACH B: named standalone pairs (the alternative for wider modules)
Split a large module into two named modules, each with its own identity. E.g.:
- SANDS + SANDSDISTRIBUTION (probability controls + atom/warp/tilt depth — currently in popup).
- STRAITS + STRAITSSUBGATES (global articulation + subgate per-voice thresholds).
Each module makes sense alone; connected as expanders when adjacent, but not "broken" when apart.
- PRO: graceful separation — each is a recognizable module in the browser, not an orphaned fragment.
- PRO: cleaner naming — function-specific (SandsDistribution) not lane-specific (StraitsLaneREST).
- CON: wider (needs its own panel framing/title).
- CON: two browser entries instead of "base + optional parts."
- BEST FOR: functional splits (distribution-depth vs main-controls) where each half has a clear role.

### THE CHOICE MAY DIFFER PER MODULE
Straits per-lane expanders (approach A) may stay right for Straits (the lane IS the identity).
Sands distribution depth (approach B, named pair) may be right for Sands (the functional split is clean:
"probability knobs you touch live" vs "distribution shape params you set and forget"). Don't force one
approach everywhere — the right split depends on whether the depth is per-lane (A) or functional (B).

STATUS: both candidates live. Prove via the Straits subpanel prototype and the Sands distribution-surface
need. The adjacency problem exists either way (Rack has no module grouping); approach B degrades more
gracefully but costs more panel width.
