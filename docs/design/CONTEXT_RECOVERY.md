# CONTEXT RECOVERY — read this first in a new chat / Claude Code session

Purpose: pick up the dot.modular / Monsoon design arc WITHOUT the user (Rodney) re-explaining it.
This is the single entry point for a fresh session. Read this, then `ROAD_TO_RELEASE.md` and
`DOC_PRIORITISATION.md`. There are ~139 docs in `docs/design/`; this file orients you.

Repo: github.com/Rodneyh303/Red-Dot-Modular. Work happens on **master** (pre-release; also a
`feat/microtonal` branch exists — confirm which is current). Container CANNOT build the Rack SDK —
build/render/verify is a **Claude Code** job in Rodney's env; web-chat is for design + container-safe
doc edits.

---

## THE PROJECT (identity)
- **dot.modular** (always lowercase). A **free / open-source VCV Rack v2 plugin**. A **love letter to
  SINGAPORE** — the "Little Red Dot", the port-city crossroads. Brand red = #d4001a. Modules named for
  Singapore places/landmarks whose real-world function echoes the module's function.
- Author: **Rodney** — Singapore-based quant-finance (XVA) professional + electronic musician. Brings
  quant rigor (correlation, reproducible/reversible generators, model-validation discipline) + musical
  intent. Wife **Lisa**.
- **THESIS**: an instrument that bridges musical worlds via a general engine. **Maqam is ONE EXAMPLE**,
  not the subject (Singapore is). Aims to honour many traditions (raga, gamelan, Western, rhythmic) —
  faithfully, with humility ("affects sayr, not implements sayr"; "rotation isn't mode").
- Central aesthetic: **mixing ORDER and CHAOS** (hence reversible randomness, correlation, self-
  reference like "Change Change Alley").

## THE MODULE ECOSYSTEM (host + expanders on a shared correlation engine)
- **Monsoon** = the host sequencer/quantiser. The core engine.
- **Change Alley (CA)** = the correlation engine (Philox-driven scatter; the trading-passage → exchange).
  16x16 voice pin matrix + verb controls (collapse/rotate/reflect/scatter).
- **Sands** = per-voice/per-lane SHAPING (LOR run-length, spread, variation, legato). The most control-
  dense, hard-won subsystem. Mono + Macro panels, tabbed/per-voice editing.
- **Straits** = poly-voice expander (sets voice count; per-voice knobs live here).
- **Causeway** = CV / modulation path expander.
- **Keppel** = CV → MPE MIDI OUT (harbour/terminal; splits voice into nearest-12-TET note + pitch bend).
- **Sikit** = tuning source, **12-note-only** .scl loader (publishes cents to the shared TuningTable).
- **Colonnades / Duo** = tuning+scale AUTHORING expander (cents+weight+mask); Colonnades up-to-12, Duo
  up-to-24. 1:1 with its Monsoon.
- **Shophouse / Shophouse Micro** = custom-scale / non-12 tuning route.
- **Emerald Hill** = double-width Shophouse (4×24-note .dmtune slots) — named for the Peranakan
  conservation street Rodney lived on. PARKED.
- **Intertropical, Lantern (illuminates/observes the system), Junction/Raffles (modulators)** = others.
- **Esplanade** (pitch piano-roll editor — the arts centre) + **Zouk** (gate editor — the club) = NEW
  authoring editors (V1). **Woodlands** = candidate name for a possible MPE-in module (mostly not needed
  — see below).

## KEY MECHANISMS (how the engine works)
- **Philox RNG**: a keyed BIJECTION. Draws = pure function of (SIGNED counter, key) → `at(N)` addressable,
  `at(N-1)` re-derives the previous draw EXACTLY, no reseed. This is WHY reverse/scrub/dice are cheap and
  exact. Per-stream keys by additive offset: STREAM_RHYTHM=0, MELODY=1, CA=2 (add SOURCE_SELECT=3 for
  q-mix). Reserved nonce ctr[2..3] for extra dimensions (e.g. per-voice).
- **Reversibility is intrinsic** (signed counter). Phase engine: continuous forward + within-phrase
  reverse DONE; cross-phrase jump/scrub regeneration is the one open phase item.
- **PPQN** is settable **24/48/96** (all multiples of 24 → every note value resolves to integer pulses;
  24 covers 1/32 + triplets). Anything non-multiple-of-24 = major change.
- **Lane length is 1..16**, baked into the counter (`DNA_LCM = LCM(1..16)×2`). CANNOT exceed 16 without
  rebuilding the counter → get length via MORE 16-lanes, not longer lanes.
- **Tuning is a swappable layer**: quantise/generate/blend operate in DEGREE space against the shared
  TuningTable. Microtonality is OPT-IN — attach Colonnades/Duo/Sikit (claim → publish cents); nothing
  attached → 12-TET default. Same "general engine, specificity injected underneath" theme as sayr.

## ARCHITECTURAL PRINCIPLES ESTABLISHED (apply these everywhere)
1. **CHECK THE CODE, not the docs.** Docs drift; code is truth. This session repeatedly found things MORE
   done / different than docs said (reverse-phase, jump/scrub replay, enabled mask, deparam, CA own-draws,
   Melodicer=Vermona-not-Vult, VCV-core-doesn't-do-MPE-in). Always grep before claiming.
2. **UNDO IS USER-ONLY.** Only USER actions push undo history; MODULATION does not (else the stack fills
   with automation churn). Standing principle, whole-plugin. **AUDIT flagged** (verify no module pushes
   undo for modulation-armed commits — suspected latent bug in CA's applyPendingTransforms). See
   `UNDO_AND_REVERSIBLE_AUTOMATION_PRINCIPLE.md`.
3. **REVERSIBLE-AUTOMATION special class.** Automation isn't undoable — EXCEPT where cheaply reversible by
   construction: (a) Philox-invertible (dice-reverse; free, unbounded) or (b) small-cacheable state (CA
   true-reverse; cheap, bounded by cache). These get PARALLEL reversal affordances, not user-undo.
4. **State-dependent scatter** (CA scatter TRANSFORMS current pins, path-dependent) — chosen for musical
   perturb-and-compose; the snapshot ring is the deliberate cost. Undo restores STATE (snapshot); reverse-
   dice reverses the DRAW (not state). Different mechanisms, different needs.
5. **Follow hard-won PATTERNS** (deparam = modulatable values w/ edit-vs-modulated split; undo; lock mode
   axisMask; UI MVC). New features are ARRANGEMENTS of existing engineering, not new engineering — low
   novel-risk. Follow them FAITHFULLY (they're hard-won because deviating hurt before).
6. **PRE-RELEASE: breaking changes are FREE.** No users, no saved patches. Any "deferred: needs migration"
   / "don't break patches" caution is VOID — do the clean thing now (param IDs, mode letters, preset
   format, panel HP). Lock formats deliberately only when release approaches. **Never raise breakage.**
7. **Idiom vs level**: share the mechanism/idiom (e.g. knob-vs-per-step-probability), keep the semantic
   level distinct (shaping vs selecting). Same-idiom controls live together (q-mix on Sands next to
   rest/accent).
8. **Shareable = data/sources, not authoring surfaces.** Read-only sources (Sikit, the editors) can feed
   MANY Monsoons; authoring surfaces w/ per-consumer write-back (Colonnades) are 1:1.
9. **Scope growth is CORRECTION not creep** when it's coherent (a pillar) AND rides existing patterns.
   The quantiser modes are a V1 PILLAR (Monsoon as universal transformer of any external gate/CV) —
   ~half the value proposition. Stage WITHIN the pillar, don't trim it.

## THIS SESSION'S BIG ARC: the q-mix feature (all captured, mostly on master)
See `RANDOM_VS_INPUT_MODULE_CONCEPT.md` (the full spec, top-to-bottom).
- **q-mix** = per-VOICE probability of taking a GENERATED note vs the external INPUT note, in quantiser
  mode. A composed↔generated FREEDOM GRADIENT across voices = controllable HETEROPHONY (voice 0 faithful,
  upper voices free). Both sources route via Change Alley (input CV is a CA melody source).
- **Mechanism**: a 4th Philox stream (STREAM_SOURCE_SELECT=3, voice in the reserved nonce). Reversible,
  zero-storage. **Separate dice from melody** (so you can vary "which notes" vs "where they interleave"
  independently).
- **It's the pitch-side of an external-input SYMMETRY**: external GATES ← rhythm/legato/rest/accent;
  external MELODY CV ← q-mix. q-mix COMPLETES the pitch side (the gate side already had its modifiers).
- **Idiom** = Sands-item (knob vs per-step probability). **Placement** = Sands lane, position 3 (data-flow
  adjacency: it blends rows 1-2 = melody+octave). **Build order**: MVP the blend + LISTEN, then add Sands
  refinements.
- **q-mix is ENOUGH** for the pitch side (scale + fader + octave modulation already exist).
- **Octave quantise** (verified): nearest active degree within INPUT-octave ±1, fader-weighted, register-
  preserving. Optional context-menu "**fold to octave range**" (min/max via 2 Monsoon faders, FOLD not
  clamp; same min/max govern generation AND quantisation → unified window; bassline + higher-octave-
  flourish example). Same/different octave = a tunable axis (window width).
- **Distribution across modules**: Big5→**Big6** on Monsoon (q-mix a headline modulatable); poly knobs →
  Straits; CV → Causeway; mix-tap → Sands Macro; gate inputs (Mode B) too. Panel grows (fine pre-release).
- **Reference**: Vermona **Melodicer** (C/D quantizer modes) — convergent on fader-width-as-capture + 5V +
  clock/gate split; silent on octave (our behaviour stands). We EXCEED it (per-voice blend, poly,
  microtonal). Don't need to match it.
- **MPE-IN**: probably NOT a build — use a THIRD-PARTY MPE module (VCV core doesn't do it cleanly, TBC
  which). Validate via a **round-trip test** (write out via Keppel → read back → require <~1-2 cents
  error). Reverse-Keppel (Woodlands) only if the round-trip fails. All V1.

## CA PIN PLANES + "CHANGE CHANGE ALLEY" (V1)
- Pin colours (CODE truth): **white = rhythm, red = melody**, concentric. (Old docs said teal — stale.)
- **Green** = the new q-mix SOURCE-SELECT pin plane on the 16×16 voice matrix (per-voice). **Yellow**
  reserved for a future per-voice stream. EMS matrices historically used white/red/green/(yellow) — so a
  3rd/4th pin is idiom-authentic; render positional-in-cell.
- Adding q-mix takes CA's scatter draw **8 → 12** dims = Intra/Inter × {rhythm,melody,q-mix} × dom/cod =
  a 2×3×2 PRODUCT (not a flat 12-D).
- **"Change Change Alley"** = correlating CA's OWN scatter draws = THREE SMALL per-axis grids on one side
  of CA (2×2 dom/cod, 2×2 Intra/Inter, 3×3 stream-types), one pin per row (diagonal = independent default;
  3×3 = 27, 2×2 = 4 configs), reusing CA's pin idiom one meta-level up. Self-reference is ON-THESIS (order/
  chaos on itself). **Promoted to V1.** Details in `RANDOM_VS_INPUT_MODULE_CONCEPT.md`.

## CA VERB STRUCTURE (for panel work)
4 verbs: Collapse, Rotate, Reflect, Scatter. Per (stream, side): **jacks = 10** (Collapse2/Rotate2/
Reflect2/Scatter4 — dom/cod each; scatter +fwd/rev because scatter is the only reversible verb);
**buttons = 10** (twins); **knobs = 6** (Collapse2/Rotate2/Reflect1/Scatter1). Jacks & knobs have DIFFERENT
distributions — lay blocks independently.

## CA PANEL LAYOUT PROBLEM (handed to Claude Code) — see `CA_PANEL_THREE_STREAM_LAYOUT.md`
q-mix adds a 3rd stream → 12-rows-per-side, exceeds the ~8-row norm. Constraints: 12 rows + jacks-on-edges
+ comfortable size; eurorack height is FIXED (128.5mm). Layout is GENERATOR-OWNED (`gen_change_alley_v2.py`
+ must-match .hpp constants).
- **Plan A** (try first): CURRENT mirrored layout (L=Intra/R=Inter) + SMALLER jacks / tighter row pitch.
  Least disruption. Try tighter CTRL_ROW_H with stock PJ301M first; if not, custom smaller-SVG port widget
  (jacks scale worst — don't transform-scale PJ301M).
- **Plan B** (fallback): 6-COLUMN reorg (lose symmetry) = (rhythm,melody,q-mix)×(intra,inter) columns, in
  component-type sections (jacks 6×10, buttons 6×10, knobs 6×6) + matrix + CCA submatrices + mod jacks.
  Bigger panel; also SIMPLIFIES the generator (uniform grids).
- **Tabbing RULED OUT** (breaks jacks-on-edges). Edgeless expander = further fallback (has its own edges).
- **The 2 CA poly-mod inputs** (GRAIN/STEP_POLY_IN) don't scale to 3 streams → DEFERRED (cut / move-to-
  Causeway / keep-if-room), decide after the reorg. Not cut yet.

## OTHER OPEN / PARKED ITEMS
- **Monsoon key/seed-OFFSET knob** (`CRAB_CANON_RECIPE.md`): the counter-address primitive — expose Philox
  addressing as a control so Monsoons sharing a seed sit at OFFSET positions → CANONS / crab / retrograde.
  Documented MUST-HAVE, NOT built. Lean RELATIVE offset. Timing: **modulatable-live vs offset-at-reseed =
  TBC** (unresolved). IF modulatable: user tweaks undo, modulation doesn't (P2/P1). Engine already has the
  addressability; needs a control surface.
- **CA true-reverse** (`CA_DICE_COUNTER_MODEL.md`): a phrase-granular replay of the recorded state
  trajectory backward = a PERFORMANCE retrograde (distinct from user-undo + dice-reverse). Buffer is TINY
  (states change once per phrase; 1MB ≈ ~10,900 states ≈ hours; 10MB ≈ days) — memory is a non-constraint,
  cap by musical taste (tens–hundreds of phrases). Reuses snapshot data at greater depth. It's a modulation-
  class action → not on user-undo (P1), reversible by construction (P2). PROPOSED, not built.
- **Snapshot ring** = 16-slot SPSC audio→UI hand-off (~3KB), NOT the undo depth (that's Rack history).
- **Tonic/transpose** (`TONIC_TRANSPOSE_BUILD_BRIEF.md`): Sikit is 12-only so transpose is safe there
  (modal-rotation on unequal tunings; relabel "root"?). Non-12 path (Micro/Emerald Hill) needs rotateMaskN
  generalisation. Rotation is a family: probability(emphasis)/mask(membership)/tuning(intonation)/note vs
  octave/rhythm — a "rotate everything" compound existed (commented DNA-rotation menu). Scale rotation
  arises because .scl has no fixed root (rooting is a free CHOICE the loader makes).
- **sayr mapping** (`presets/maqam/README.md` + `LAUNCH_INTENT_AND_STORY.md`): the general modulation
  engine's dimensions map onto maqam sayr — 5 of 6 sayr components map to DIRECT built primitives (starting
  note, emphasis, pauses/ghammaz, modulation, direction/PENDULUM=the arch), region-ordering is direct in
  QUANTISER mode (external note order = the pathway). Pendulum direction mode = the archetypal maqam arch.
  Thesis/launch material.

## RELEASE PLAN — see `ROAD_TO_RELEASE.md` (the hub) + `DOC_PRIORITISATION.md` (triage of ~139 docs)
Order: **functionality first, then panels, then release deliverables.** Phase-1 pillar = **finish the
quantiser modes** (MODES_C_D_QUANTIZER_PRERELEASE — verify true state first, docs drift). Then tonic/
transpose, Keppel/MPE, accent family, Sikit multi-reader. CI is green on 3 platforms. Define the explicit
"V1 ships" cut line (scope grew this session — sanity-check shippability, stage within pillars).

## HOW TO USE THIS IN A NEW SESSION
1. Read this file, then ROAD_TO_RELEASE.md + DOC_PRIORITISATION.md.
2. Re-clone each session (filesystem resets). Work on master (confirm vs feat/microtonal).
3. GREP THE CODE before trusting any doc's status — the meta-principle.
4. Build/render/verify = Claude Code (container can't build the Rack SDK).
5. Apply the principles above (esp. user-only-undo, follow-patterns, pre-release-breaking-is-free,
   scope-is-correction-not-creep, check-the-code).
