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
  * KILLING MACRO KILLS SENDS TOO (Rodney 2026-11-22). The editor collapse is NOT "macro = page 0"
    (Rodney's correction 2026-11-23: a page is ONE VOICE's view; macro is the global default UNDER all
    16, a different KIND of thing — not a 17th voice tab). The MODEL collapses cleanly (one per-voice
    store + global-default row + follow) and the JACKS collapse (poly convention); the EDITOR does NOT
    collapse for free. It needs a GLOBAL-EDIT AFFORDANCE distinct from the voice pages — the least-bad
    is an explicit "ALL / GLOBAL" tab (semantically distinct from V1..V16) . REFINED (Rodney 2026-11-23 — separate macro CONTROLS from the VIEW; do not conflate them). Once
    macro is the global-default ROW in the per-voice model, BOTH the view and the controls just POINT AT
    A ROW: view voice N = render row N; view global = render the global-default row (same lane renderer);
    edit voice N = write row N; edit global = write the global-default row (same controls). So the editor
    is ONE editor with a ROW SELECTOR of 17 targets: V1..V16 + GLOBAL. GLOBAL is NOT "voice 0" (not
    voice-shaped) — it is one more SELECTABLE ROW the existing view+controls aim at. This is MOSTLY REUSE,
    cheaper than a bespoke global-edit mode: you add a selectable target, not a new surface.
    VIEW-WHILE-EDITING-GLOBAL: point the VIEW at the global row too (render the global pattern directly),
    NOT at an actual voice — this avoids the override blind spot (editing global while viewing an
    OVERRIDING voice would show no change). Rodney's three selector options (inherit last voice / a macro
    channel knob / a global toggle) are just different SELECTORS for the GLOBAL target; prefer any that
    drives the VIEW to the global row (avoid plain inherit-last-voice for the blind-spot reason). REQUIRED
    for BOTH the Sands probability visual AND the Distribution (2026-11-23) — the SAME row-selector
    affordance reused on both, not two bespoke designs. Net editor cost: "add GLOBAL as a 17th selectable
    row for the existing view+controls" — modest, mostly reuse (the earlier "not a cheap win" over-swung).
  * THE CV MODEL: a MONO CV input (Rack auto-replicates it across all poly channels = the global
    signal, free) PLUS a POLY CV input (per-voice). Each voice gets TWO per-channel attenuverters: one
    for how much of the GLOBAL signal it takes, one for its own POLY signal. Effective(v) =
    global×globalAtten[v] + poly×polyAtten[v]. This is Causeway's (perVoiceAtt + globalAtt) pattern
    generalized — candidate for a SHARED primitive across Causeway and the unified Sands (Causeway was
    already slated to "end up a bit like Straits").
  * WHAT SURVIVES OF SENDS (its 3 parts redistribute): per-voice mix-in sends → the per-voice
    global-amount attenuverter (above); distribution-param send amounts → the Distribution's own
    per-voice depth knobs (the 56 East-Distribution depths already decided); PRE/POST taps → LIKELY
    DROPPED (Rodney 2026-11-23). PRE/POST was a SEND-ROUTING concept (where in a chain the global
    injects). Once the global is just a MODULATION SOURCE each voice scales with its own attenuverter,
    there is no injection point — only an amount — so the taps have nowhere to live. They were an
    artifact of the "send" framing, not load-bearing. Sends reduces to ONE kind of control: a per-voice
    attenuverter on the global mod. (Retain a pre/post control only if a concrete use appears.)
  * KEEP THE GLOBAL PROB-OUT GROUP (Rodney 2026-11-22 — the one real asymmetry). Both Macro and East
    already emit poly prob CV outs (PROB_OUT_START): Macro = the global-mod value across voices, East =
    the resolved per-voice follow/override value. In the unified model BOTH reads still exist (global =
    the global-default row + its mod; per-voice = effective[v]), so keep BOTH output groups on the one
    Sands. This is the ONE output that does NOT fold into the per-voice poly out: on the INPUT side the
    poly convention merged Macro/East (mono cable = global), but on the OUTPUT side you cannot borrow
    ch0 for the global because ch0 is V1 — so the global prob out stays a DISTINCT jack group. Not a
    capability loss: it is a trivial read of the global-default row, retained deliberately. Its value is
    being a STABLE GLOBAL REFERENCE independent of per-voice overrides (when voices follow macro the
    East out already equals it; it earns its separate existence in the overridden case — exactly when
    both signals are wanted).
  * END STATE: one Sands visual (global page + 16 voice pages), one Distribution (same page model), NO
    Macro module, NO Sends module — Sends' function redistributed to per-voice CV attenuverters +
    Distribution per-voice depth. A real module-count deletion, not a reshuffle.
  * EASES A FUTURE 64-STEP MODE (Rodney 2026-11-22). The engine mask/storage layer is ALREADY 64-ready
    — kStepStorage <= 64 ("uint64 covers the 64-step ceiling, no second widening"), kMaskLenMask has a
    `kMaskRing >= 64 ? ~0ULL` branch; STEPS_32 (16→32) deliberately pre-sized for an eventual 64. The
    unification + fold remove the REMAINING friction: (1) one per-voice model is a SINGLE step-dimension
    bump to 64, not two tiers' arrays + the twin identity/seed reconciliation (the characterization
    tests carry over); (2) one editor (Macro as a page on East) extends to 64 lanes once, not twice;
    (3) the fold gives 16/32/64 as PANEL WIDTHS for free — width is derived from content, so a longer
    pattern widens the lane band, and the Straits-style viewport means 64 steps can be a wider panel OR
    a 32-wide window scrolling across 64. STILL SEPARATE: the ring-lamp display (the "twice around"
    16-lamp packing of 32) needs a rethink for 64 — display only, orthogonal, gates nothing.
  * RISK: touches the bug-prone resolve paths (16-triage, macro-lane-seed, voice-frame-read) — do it
    deliberately against the characterization tests, after the fold. The fold SETS IT UP: once Sands is
    bands over one store, collapsing the two tiers is a well-scoped refactor rather than a tangle.
- Causeway's home (Monsoon group vs Straits/poly group).
- Straits: scrolling knob band vs raising kMaxHp (the cap breach is real).
- Change Alley: the exact 3/3/2 jack set and the max number of sets.
- Does any group allow 2+ of a member beyond Interchange? (Default rule: 0/1 unless stated.)
- Mutual-exclusion is a DESIGN rule today, not code-enforced for tuning authorities — the group
  module should enforce it (one visible at a time) even though loose modules don't.
