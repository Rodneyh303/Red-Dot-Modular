# CC PROMPT — Straits subgate consumer-group knobs (ratchet + ghost) — next Straits step

Design source: STRAITS_POLY_SUBGATE_KNOBS.md (the consumer-group model). This is the CC-executable spec
for the NEXT Straits build: the subgate-tier threshold knobs. It UNBLOCKS gate-step-advance Stage 3
(CC_PROMPT_GATE_STEP_ADVANCE_SPEC.md) — the ratchet/ghost advance thresholds ARE these knobs. Branch off
lane-expander-refactor.

## WHAT THIS ADDS
Subgate-tier consumer-group thresholds, per subgate decision. Subgate decisions include (at least):
ratchet-rest, ghost-rest, ratchet-advance, ghost-advance, subgate-legato. (ADVANCE ones are the Stage-3
step-advance thresholds; REST/LEGATO ones are the per-subgate articulation.) Each is a consumer of a draw
event at its subgate level — SAME-event consumers read the SAME sampled value; different events (main vs
ratchet vs ghost) are independent draws → separate thresholds express different rates (not fake
independence).

## THE THREE-WAY SOURCE per subgate decision (Rodney)
For EACH subgate decision, the per-voice value resolves by this precedence:
1. OWN POLY-KNOB SUBPANEL ATTACHED → use it (per-voice subgate control). Highest priority; auto-selected by
   ATTACHMENT (own subpanel present → it wins, no switch needed).
2. NO own subpanel → a SWITCH chooses between:
   (a) GLOBAL STRAITS KNOB — one value, all voices (the subgate base knob on Straits), OR
   (b) FOLLOW MAIN POLY KNOBS — the subgate decision inherits the voice's MAIN lane per-voice value
       (ratchet-rest follows the voice's main REST; subgate-legato follows main LEGATO; etc.).
So: own-subpanel (if attached) > { global-Straits-knob | follow-main-poly } (switch). The switch only
matters when the own subpanel is NOT attached (attached always wins).

### The switch = a STRAITS CONTEXT MENU for now
Per subgate decision (or per subgate group): a context-menu choice "source: global / follow main". It is a
SET-AND-FORGET routing choice, not a performance control → context menu is right for now; can graduate to a
panel switch later if it earns one. Default: FOLLOW MAIN (least surprising — subgate inherits the main lane
until the user opts to a global subgate value or attaches a subpanel).

### This mirrors the Macro-default / per-lane-opt-in pattern
Same SHAPE as the existing distribution default/override: a precedence of sources with a fallback. One
resolved value per (voice, subgate-decision) from a defined precedence — resolver discipline.

## PANEL LAYOUT
- GLOBAL subgate knobs: on the Straits base (or a Straits subgate base section) — one knob per subgate
  decision, Monsoon-mirrored sizing.
- OPTIONAL per-voice subgate subpanel: snapped (new-Straits snapping, CC_PROMPT_STRAITS_RESNAP_HARDENING +
  the SandsSnap-fix invariants — stable owned-ID list + unconditional reposition-by-ID). Per-voice knobs
  for the subgate decisions. Attaching it = tier 1 (own poly knob wins).
- CAUSEWAY MOD COVERAGE: every new Straits knob needs its Causeway mod path (a Straits knob without its mod
  path = a modulation-layer seam). Add the knob AND its Causeway mod together.

## RELATION TO STEP-ADVANCE STAGE 3
The ratchet-advance and ghost-advance thresholds built here ARE the Stage-3 thresholds in
CC_PROMPT_GATE_STEP_ADVANCE_SPEC.md. Once these land, step-advance Stage 3 (three-level main/ratchet/ghost)
is unblocked: each subgate level reads its advance threshold through the three-way resolution above, same
u-draw discipline, same one-playhead constraint.

## DONE =
- Per-subgate-decision three-way resolution: own-subpanel > {global-Straits | follow-main} (context-menu
  switch), default follow-main. One resolved value per (voice, decision).
- Global subgate knobs on Straits + optional snapped per-voice subgate subpanel (snapping per the fixed
  invariants). Causeway mod path for each.
- Test: the three-way precedence (attach subpanel → tier 1; detach + switch global → tier 2a; detach +
  switch follow-main → tier 2b); default = follow-main.
- run_all.sh green. (Unblocks step-advance Stage 3.)

## NOT THIS SPEC
- Step-advance Stages 1-2 (independent, build separately). The distribution expander. Phase-hybrid ideas.

────────────────────────────────────────────────────────────────────────────────
## CLARIFIED — no main-knob copies on Straits base; add subgate globals; the full resolution (Rodney)

### Do NOT copy Monsoon main-lane knobs to the Straits base (for now at least)
A global MAIN-lane knob (legato/rest/variation/accent) on the Straits base would be a redundant Monsoon
copy needing sync (like the existing Straits V1 knobs). The global main value already lives on MONSOON. So
do NOT add Straits-base copies of the main knobs. (May revisit for ergonomics later; not now — Straits
space is limited.)

### DO add subgate GLOBAL knobs on the Straits base
The subgate global knobs are GENUINELY NEW values (how ratchet/ghost behave GLOBALLY — distinct from the
main lane, e.g. "ghosts rest more than main notes"), NOT Monsoon copies. These belong on Straits base. This
is why the subgate source is a real THREE-WAY (the global-subgate value is distinct from follow-main).

### FULL RESOLUTION — main lanes and subgates

MAIN LANE (legato/rest/variation/accent) per-voice value:
- Straits poly subpanel ATTACHED for that lane → its per-voice knob.
- NO Straits subpanel → FOLLOW MONSOON (the main knob). "No Straits = no poly" → every voice uses the
  Monsoon global main value. (No Straits-base global copy involved; Monsoon IS the global.)

SUBGATE decision (ratchet/ghost: legato, rest, step-advance) per-voice value — THREE-WAY:
- Own SUBGATE poly subpanel ATTACHED → its per-voice subgate knob.
- Else, SWITCH (Straits context menu, default follow-main):
   (a) GLOBAL SUBGATE KNOB (the distinct Straits-base subgate value), OR
   (b) FOLLOW MAIN POLY — the subgate follows the MAIN lane's resolved per-voice value (which is: main
       Straits poly knob if attached, else Monsoon main knob).

### THE DEGENERATE CASE (no Straits at all) — everything follows Monsoon
With NO Straits attached (no main poly, no subgate knobs):
- Both subgates' LEGATO follow MONSOON LEGATO (etc. for rest/accent).
- Both subgates' STEP-ADVANCE follow MONSOON VARIATION (Variation = the step-advance lane in gate mode).
So without Straits the whole subgate tier transparently inherits the Monsoon main values — the subgate
knobs/choice only START to matter once a main lane has poly knobs (a Straits subpanel) OR a subgate global/
subpanel is set. Until then, pure Monsoon-inherited behaviour, no new surface needed.

### IMPLICATION for "follow main" vs "global subgate"
follow-main only DIFFERS from global-subgate when the main lane has per-voice values worth following (a
Straits main poly subpanel attached). Without that, both resolve to the Monsoon main value — identical. So
the switch is MEANINGFUL only when main poly knobs exist; harmless (no-op choice) otherwise.
