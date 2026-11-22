# Group-panel inventory (fold modules) — Rodney 2026-11-22

The fold/group panels bundle a HOST plus the expanders that STRICTLY belong with it, usually
0 or 1 of each (some 0/1/2), several mutually-exclusive within a bracket. This inventory is the
input to the port-id freeze (every member's ports declared up front in the group module's
config(); a member popped out just reveals its already-declared port widgets). Cardinalities and
brackets are Rodney's design rulings; the code facts behind them are cited where checked.

Legend: [host]=always 1. {a|b|c}=mutually exclusive, pick 0 or 1. JACKS=brings ports (edge-pinned
panel). KNOBS=knob/fader only (interior band). Verify exact port counts at freeze time.

## The fold criterion (Rodney 2026-11-22) — association cardinality
A module FOLDS into a host's container iff it associates with EXACTLY ONE host instance (1:1). A
module stays a standalone ISLAND iff it serves MANY hosts (1:many) or is a free-aimed VIEW. This is
not a preference — folding a 1:many module into one host's container would physically bind it to that
instance and destroy the multi-host/aim capability that is its whole purpose. (An island may still
have its OWN internal fold — e.g. Change Alley bundling its jack/attenuverter subpanel — it just does
not fold INTO a Monsoon.)

### Islands (NOT folded into a host)
- **Change Alley** — earmarked to drive MULTIPLE Monsoons; island. (May get its own internal fold:
  break its jacks/attenuverters to a subpanel — self-contained, one CA instance.)
- **Cantonment** — likely same (multiple Monsoons); island.
- **Lantern** — a VIEW aimed at different Monsoon / Intertropical; island.
- **Keppel, Woodlands, the independent Phaser** — utilities, more independent; islands.
- **Monsoon** — the HOST, not an expander (it anchors a fold group, it is not folded into one).

### Fold groups (1:1 host association) — detailed below
Monsoon group; Sands family (Macro/East + Distribution + Sends — see OPEN); Straits group (+ Intertropical);
Causeway (future, Straits-like poly fold).

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
- Causeway: a FUTURE Straits-like poly fold of its own (Rodney 2026-11-22), not a Monsoon-group member.

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
- **0/1 Intertropical** (Rodney 2026-11-22) — reads its input from Straits and associates with ONE
  Straits instance, so it folds into THIS group (the scene sequencer / arranger; JACKS: poly outs).
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

## 5. Change Alley — ISLAND (NOT a Monsoon fold; see criterion). Its OWN internal fold only:
- Change Alley V2 **[host]** — 16x16 pin matrix, rhythm/melody/q-mix source planes, per-stream
  reverse jacks + scatter trig jacks. Big.
- BREAK OUT jacks/attenuverters to a SEPARATE subpanel (size) — an edge jack panel.
- MAYBE allow MULTIPLE SETS of the "3/3/2 melody/rhythm/q-mix CV in/out" block — OPEN: pin exactly
  which jacks the 3/3/2 names (the domain/codomain trigs? the reverse jacks?) and how many sets
  (2? 4?), since each set multiplies the port enum and must be frozen up front.

## Open questions (resolve before the port freeze)
- SANDS FAMILY: TWO groups FOR NOW (Macro + Sends + Distribution; East + Distribution) — Rodney
  2026-11-22. With a known FUTURE SIMPLIFICATION to ONE group, pursued AFTER the fold proves out on
  Straits (do not block the fold on an engine refactor; do the refactor against the characterization
  tests). The unification (recorded so it is not lost):
  * The ONLY real Macro/East difference is "applies across all voices" (Macro) vs "per-voice override
    or follow-Macro" (East). That is ONE model: a per-voice value + a GLOBAL DEFAULT row + a per-voice
    follow flag — effective(v) = follow[v] ? globalDefault : own[v]. The engine already resolves this;
    today it is just expressed as TWO TIERS with twin code (effectiveMacroRowLor vs effectiveVoiceLor,
    macroRowIdentity vs voiceLorIdentity, the macro-lane-seed fix that exists only to keep the two seed
    laws identical). One per-voice store + one global-default row removes the twinning.
  * SAME REFACTOR LINE as killing "mono is special" (mono became voice 0): killing "macro is special"
    makes Macro the global-default row of the per-voice model. Step two of the same simplification.
  * THE JACKS collapse fully via the Rack poly convention: ONE poly input per lane, a MONO cable
    broadcasts ch0 to all (= the Macro/global behaviour) and a POLY cable targets ch==voice (= the East
    per-voice behaviour). This IS the East convention already (ch==voiceSlot, 1-channel broadcasts,
    StraitsEastSandsVisual.cpp ~1461). So Distribution's "global vs poly" is one jack read two ways.
  * THE KNOBS do NOT collapse to one control — you still need a global default + 16 overrides + follow
    flags (that is the feature). But in a fold that is just a GLOBAL band + a PER-VOICE band over one
    store, so the module count still drops to ONE Sands host (not Macro-visual + East-visual).
  * RISK: touches the bug-prone resolve paths (16-triage, macro-lane-seed, voice-frame-read) — do it
    deliberately against the characterization tests, after the fold. The fold SETS IT UP: once Sands is
    bands over one store, collapsing the two tiers is a well-scoped refactor rather than a tangle.
- Causeway's home (Monsoon group vs Straits/poly group).
- Straits: scrolling knob band vs raising kMaxHp (the cap breach is real).
- Change Alley: the exact 3/3/2 jack set and the max number of sets.
- Does any group allow 2+ of a member beyond Interchange? (Default rule: 0/1 unless stated.)
- Mutual-exclusion is a DESIGN rule today, not code-enforced for tuning authorities — the group
  module should enforce it (one visible at a time) even though loose modules don't.
