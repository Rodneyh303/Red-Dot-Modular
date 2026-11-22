# Group-panel inventory (fold modules) — Rodney 2026-11-22

The fold/group panels bundle a HOST plus the expanders that STRICTLY belong with it, usually
0 or 1 of each (some 0/1/2), several mutually-exclusive within a bracket. This inventory is the
input to the port-id freeze (every member's ports declared up front in the group module's
config(); a member popped out just reveals its already-declared port widgets). Cardinalities and
brackets are Rodney's design rulings; the code facts behind them are cited where checked.

Legend: [host]=always 1. {a|b|c}=mutually exclusive, pick 0 or 1. JACKS=brings ports (edge-pinned
panel). KNOBS=knob/fader only (interior band). Verify exact port counts at freeze time.

## 1. Monsoon group
- Monsoon **[host]** — the main jacks (gate/CV/accent in+out, the DNA/poly chain). JACKS.
- Tuning authority: **0/1 of {Colonnades | Colonnades Duo | Sikit}** — mutually exclusive (one
  tuning/scale authority per Monsoon; single writer of weight[]/cents[] — MicroTuning pairing).
  KNOBS (faders/cents) + file load. (Colonnades=12 faders+cents; Duo=24; Sikit=12 cents only.)
- **0/1 Junction** — Big-5 pattern-knob CV mod (note value, variation, legato, rest, accent). JACKS.
- **0/1 Raffles** — dice/draw CV (slew/mix) + die-action gates. JACKS.
- **0/1/2 Interchange** — semitone + octave CV mod. TWO is meaningful ONLY with Colonnades Duo:
  each Interchange drives one 12-degree half (targetHalf 1 -> 0..11, 2 -> 12..23); a 2nd on a
  12-degree tuning is INERT (verified MonsoonInterchangeExpander.hpp). JACKS.
- Scale/tuning scene: **0/1 of {Shophouse | Shophouse Micro}** — mutually exclusive (Shophouse =
  12-TET scale-list fronts; Shophouse Micro = .dmtune fronts for the Colonnades family). Each has a
  front-select CV in + knobs. KNOBS + 1 JACK (the index CV).
- REVERSE: NOT an expander (Rodney 2026-11-22, superseding the earlier Direction/Reverse-expander idea).
  Reverse direction lives on MONSOON ITSELF as a new GATE 4 input (output gates shift one column right;
  G4 next to G3, assignable like G3, only target = reverse toggle; phrase-boundary cadence). See
  CC_PROMPT_CLOCK_GATE_REVERSE.md. No group-panel member for it.
- OPEN: Causeway (poly REST/ACCENT CV mod) — belongs with the POLY/Straits side, not here. Confirm.

## 2. Sands Macro group
- Sands Macro (StraitsSandsMacroVisual) **[host]** — 39 in / 7 out. JACKS.
- **0/1 Sends** (SandsMacroSendSubpanel) — 98 store-backed knobs, NO ports. KNOBS (interior, flexible).
- **0/1 Distribution** (Macro role) — 56 in, read at channel 0; Macro role packs to 8 poly by lane.
  JACKS + the 56 NEW depth knobs (need a store + test, per the plan).

## 3. Sands East group
- Sands East (StraitsEastSandsVisual) **[host]** — 47 inputs, voice-poly (not packable). JACKS.
- **0/1 Distribution** (East role) — voice-poly: 56 jacks, channel = voice slot (the per-voice-mod
  prompt). JACKS + 56 depth knobs.

## 4. Straits group  (EXCEEDS the 120 HP cap — needs a SCROLLING knob band, see below)
- Straits / Straits Base **[host]** — 16ch poly-cable outs, quantiser CV in, voice count. JACKS.
- **all subpanels** (each 0/1): lane expanders {Q-MIX, REST, ACCENT, VARIATION, LEGATO}; the 8
  subgate panels {RATCHET|GHOST} x {REST|LEGATO|ACCENT|ADVANCE}; the Q-MIX/Octave panel. KNOBS.
- **0/1 Causeway?** (poly CV mod) — JACKS; confirm it lives here.
- CONSEQUENCE: all subpanels at once blows past kMaxHp (120). FoldBandSetCore.bandWouldExceedCap
  would refuse the pop-out. Resolution needed: a SCROLLING knob band (fixed-width band, knobs scroll
  inside) rather than unbounded width — the knob panels are the scroll target Rodney named. Raising
  kMaxHp is the alternative but a 200+ HP module is unwieldy.
  RESOLVED (R, 2026-11-22): a VIEWPORT. The player picks an HP that shows N subpanels (1/2/4/6) and
  SCROLLS back and forth across the rest when there are more subpanels than fit. Rack-free core
  FoldViewportCore.hpp (test_fold_viewport): widthHpForViewport(base, visible, panelPx, gapPx) sizes
  the module; maxScroll/clampScroll/visibleRange/can-scroll drive the window and the arrows; a
  visible-count change re-clamps the scroll keeping the left edge stable. The viewport is ONE interior
  band in the FoldBandSetCore layout; the host jacks / Causeway stay edge-pinned around it. The chosen
  sizes stay well under kMaxHp however many subpanels exist. Widget half (the scroll gesture/arrows,
  drawing the window) is CC + Rodney's feel-test.

## 5. Change Alley group
- Change Alley V2 **[host]** — 16x16 pin matrix, rhythm/melody/q-mix source planes, per-stream
  reverse jacks + scatter trig jacks. Big.
- BREAK OUT jacks/attenuverters to a SEPARATE subpanel (size) — an edge jack panel.
- MAYBE allow MULTIPLE SETS of the "3/3/2 melody/rhythm/q-mix CV in/out" block — OPEN: pin exactly
  which jacks the 3/3/2 names (the domain/codomain trigs? the reverse jacks?) and how many sets
  (2? 4?), since each set multiplies the port enum and must be frozen up front.

## Open questions (resolve before the port freeze)
- Causeway's home (Monsoon group vs Straits/poly group).
- Straits: scrolling knob band vs raising kMaxHp (the cap breach is real).
- Change Alley: the exact 3/3/2 jack set and the max number of sets.
- Does any group allow 2+ of a member beyond Interchange? (Default rule: 0/1 unless stated.)
- Mutual-exclusion is a DESIGN rule today, not code-enforced for tuning authorities — the group
  module should enforce it (one visible at a time) even though loose modules don't.
