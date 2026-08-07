# dot.modular -- master plan (pre-microtonal, pre-library)

Last updated: 2026-08-04. Living doc -- update when items close.
Goal: into VCV Library 2026. Microtonal (Monsoon Micro 12/24 slider variants) is POST-library.

---

## CURRENT STATE SNAPSHOT

### plugin.json
- EXISTS at repo root. Version: 2.0.1 (correct format). Brand: dot.modular.
- LICENSE: currently "proprietary" -- MUST become a real SPDX identifier before submission.
  VCV requires SPDX (e.g. GPL-3.0-or-later). This is the one confirmed library-gate item.
- 14 modules registered, all matching source createModel() calls. Intertropical + Lantern IN.

### Module inventory (14 current + 3 planned)
Monsoon          -- ship-ready modulo rate/lock refinements
MonsoonInterchangeExpander (Interchange) -- ship-ready
Raffles          -- BUG: trial params/gates/LIVE SRC still present -- CLEANUP NEEDED
Junction         -- ship-ready
Straits          -- ship-ready
Causeway         -- ship-ready modulo lock Phase 2
Changi -> T1     -- BUG: mono jack unconfigured (i<15 not i<16) -- FIX BEFORE SHIP
Shophouse        -- ship-ready
MonsoonSandsVisualExpander    -> user-facing "Monsoon Sands"  -- ship-ready
StraitsEastSandsVisual        -> user-facing "Straits Sands"  -- ship-ready
StraitsSandsMacroVisual       -> user-facing "Sands Helix"    -- ship-ready
Lantern          -- IT-source branch needs verify+merge; otherwise ship-ready
ChangeAlleyV2    -> user-facing "Change Alley"                -- ship-ready modulo rename item 4b (undo item 5 DONE, verified in code Aug 2026)
Intertropical    -- panel theme + voice-slot grid fixed; ship-ready
[NOT BUILT] ChangiT2 -- step-gate/step-legato x16
[NOT BUILT] ChangiT3 -- IT-routed 8ch breakout (reads IT output jacks)
[not a module]   -- Scale additions (Slendro etc.)

Sands family (Rodney's intended user-facing naming): Monsoon Sands (Monsoon-attached),
Straits Sands (Straits-attached), Sands Helix (standalone/self-contained). Prefix names the host or
"Helix" for the self-contained variant; the pattern is host-first when attached, family-name-alone
when standalone. Same pattern would apply if future host-attached Sands variants appear.

### Open branches
feat/lantern-intertropical-source  -- COMPLETE, needs Rack verify then merge
feat/domain-reverse-inverse        -- COMPLETE, decide in play (test in Rack, merge or close)
feat/dice-scrub                    -- STALE (0 commits ahead of master, already merged) -- DELETE

---

## CRITICAL PATH TO LIBRARY (ordered, numbered)

### 1. LICENSE decision (BLOCKING -- choice, not a build)
plugin.json says "proprietary" -- VCV requires a real SPDX identifier. GPL-3.0-or-later is the
standard/simplest (most Rack plugins). DECIDE and update plugin.json + LICENSE.txt.

### 2. LIBRARY_SUBMISSION_CHECKLIST (build in chat -- may surface surprises)
Does not exist yet. Needs: license (item 1), plugin.json complete, slugs frozen (item 3),
clean SDK build, one VCVRack/library GitHub issue thread (title=slug, post commit hash). VCV
builds cross-platform; you only need a clean source build. Build this doc so submission is a
checklist not a guess.

### 3. Slug freeze (decision -- irreversible post-library)
Slugs cannot change post-library without breaking patch compat. Names can change freely.
- "Changi" stays (display name -> "Changi T1", slug stays "Changi") -- DECIDED.
- "ChangeAlleyV2" -- stays V2 or clean to "ChangeAlley"? DECIDE before submission.
- T2/T3 slugs: "ChangiT2" / "ChangiT3" -- add to plugin.json when built.
- MonsoonSandsVisualExpander / StraitsEastSandsVisual / StraitsSandsMacroVisual -- are these
  the permanent public-facing slugs? Decide. They're internal-sounding names.

### 4. Delete feat/dice-scrub (housekeeping)

### 4b. Pre-release NAME cleanup (housekeeping -- Rodney flagged)
Several current internal/panel names need to be updated to their intended user-facing names before
release. The collection's intentional pattern is single clean names per module (no version markers);
Changi T1/T2/T3 is the only intentional numbered family (real terminals are numbered so the metaphor
requires it). Fix in one small pass:

Accidental V2/V4 names (working context that stuck):
- `MonsoonChangeAlleyV2` / "Change Alley V2" -> `MonsoonChangeAlley` / user-facing "Change Alley".
- `SandsVisualEditorV4` (widget helper) + any "Visual Helper V4" references -> drop V4. Class rename
  is internal; user-visible name is what matters most.

Sands family user-facing names (Rodney's intended naming, panel wordmarks may not reflect yet):
- `MonsoonSandsVisualExpander` -> user-facing "Monsoon Sands" (Monsoon-attached).
- `StraitsEastSandsVisual`     -> user-facing "Straits Sands" (Straits-attached).
- `StraitsSandsMacroVisual`    -> user-facing "Sands Helix"   (standalone / self-contained).
Pattern: host-first when attached, family-name-alone when standalone.

Namespace rename (pre-rebrand artefact -- Rodney flagged):
- `namespace redDot` / `redDot::` -> `namespace dotModular` / `dotModular::`. Pre-rebrand namespace name
  that stuck across the codebase; the collection now brands as dot.modular. Scope: 52 code files use
  `redDot::` (grep to confirm current count), plus several older design docs. Pure mechanical rename
  (sed -i 's/redDot::/dotModular::/g; s/namespace redDot/namespace dotModular/g'); no semantic change.
  For OLDER design docs describing code state at time of writing, leave the redDot references as
  accurate history OR update them along with the code -- either is fine, since the docs describe
  what the code is called and the code is what's authoritative. New docs (SIKIT_CLAUDE_CODE_GUIDE,
  SCALA_FILE_AND_LOAD_UI, MICROTONAL_MASTER) already use dotModular preemptively.

Includes: class rename (where doing internal cleanup), file rename, slug update in plugin.json,
panel wordmark update, display-name references in design docs (leave CLASS-name references in
historical docs that describe actual code state at time of writing -- they remain accurate history).

Timing: do BEFORE item 3 (slug freeze), or accept the accidental slugs forever. Slug freeze is
irreversible post-library, so a rename after that is impossible without a migration. Recommend early:
small mechanical task, big clarity gain.
0 commits ahead of master. Stale. Delete the remote branch.

### 5. Changi T1 mono bug FIX (Claude Code -- ship-blocker)
MonsoonChangiExpander.hpp: constructor loop `for (int i=0; i<15; ++i)` -> `i<16`.
Label index 0 as "Mono (voice 1)", indices 1-15 as "Voice 2".."Voice 16".
Rack verify: mono jack (index 0) outputs gate/CV/accent.

### 6. feat/lantern-intertropical-source -- VERIFY + MERGE (Rack)
Branch complete: grid + piano-roll + tie-latch + voice-slot all-scenes display + debug toggle.
Rack checks:
- Grid: all 8 scene columns populate from membership selections; cursor traverses with playback.
- Piano-roll: routed voices at post-transpose pitch.
- "Verify vs raw jacks" toggle: ON/OFF -> no movement = faithful reconstruction.
- Modulate a transpose knob: single/legato notes move; tied note holds pitch for tie duration.
If clean: merge to master. Any divergence: toggle identifies which read to fix.

### 7. feat/domain-reverse-inverse -- DECIDE + MERGE OR CLOSE (Rack)
CA domain-reverse = true inverse permutation (step4 == step2, true board undo). COMPARE BRANCH.
Test in Rack: scatter, reverse, re-scatter -- does it undo faithfully? Does the gesture feel right?
Note: merging also strengthens pitch doc point 8 (reversibility claim -- domain transforms become
reversible too). Lean: MERGE, but the feel check is the gate.

### 8. Raffles trial cleanup (Claude Code -- own session, see RAFFLES_TRIAL_CLEANUP.md)
Remove: DICE_TRIAL_R/M params, RAFFLES_GATE_TRIAL/LASTTRIAL/LIVESRC inputs, G3_TRIAL/LIVESRC
gate3 targets, trialMode[] field, trial panel art. DieAction vocab already cleaned.
CAREFUL: enum shift breaks the positional rafflesGateTrig[] fire loop. Enumerate ALL value names
before editing, grep across whole tree. Panel SVG reflow after gate removal.

---

## FEATURE COMPLETENESS (before panel iteration)

### 9. Changi T2 (Claude Code)
New module. 16 voices x (STEP_GATE + STEP_LEGATO) = 32 jacks. Host-pushed by Monsoon's
OutputGenerator (same pattern as T1). Slug: "ChangiT2". Add to plugin.json.
See CHANGI_TERMINAL_SPLIT.md.

### 10. Changi T3 (Claude Code)
New module. 8 arranged channels x {gate, CV, accent, step-gate, step-legato} = 40 jacks.
Self-reading: T3's process() finds its bound Intertropical (pairing number) and reads
it->outputs[CV_OUT/GATE_OUT/...].getVoltage(ch) directly. NOT host-pushed.
Organised BY CHANNEL. Post-transpose + tie-latched (IT resolves effectiveTranspose[] first).
Shares the subgroup pairing mechanism with Lantern (item 12 below).
Slug: "ChangiT3". See CHANGI_TERMINAL_SPLIT.md.

### 11. Undo item 4 (Claude Code, see UNDO_IMPLEMENTATION_ROADMAP.md)
Items 1 (direction), 2 (LOR), 3 (knobs), 5 (CA) DONE and Rack-verified, merged to master.
VERIFIED IN CODE (Aug 2026): item 5 is implemented -- MonsoonChangeAlleyV2.hpp has scatterCounter[],
snapshot counterBefore/counterAfter (l.175/197), TransformUndoAction with both undo+redo directions
(l.842, 855, 861), ResetPinsAction (l.820), and StoreEditAction for pin edits. Nothing left on item 5.

ITEM 4 (dice undo) IS THE ONLY ONE OUTSTANDING. Verified absent: no rhythmDrawCtr/melodyDrawCtr undo
wiring, no dice action class anywhere in src/.
Scope: reversible-mode counter undo -- (before,after) scalar on rhythmDrawCtr/melodyDrawCtr.
REVERSIBLE mode only; free-run undefined. Small and self-contained -- follows the TransformUndoAction
pattern already proven for CA (capture before, mutate, capture after, action restores either side).
Previously deferred to the dice-scrub work; no longer blocked by it.

### 12. Subgroup pairing system (Claude Code + already designed)
For multiple IT + Lantern + T3 groups. FULLY DESIGNED (see MULTIGROUP_CONSERVATION_AND_CORRELATION
+ CHANGI_TERMINAL_SPLIT). Key decisions:
- Model B: numbered pairing, emergent (not first-class), source-binding only (clock orthogonal).
- Auto-assigned lowest-free number at birth; immutable (never renumber; gaps fine).
- Numbers persist in JSON; consumers store watched number; rebind on load.
- Consumers pick from existing groups (dropdown of live Intertropicals, not free integer).
- Disconnection: existing connect mark + hold-last-state (reuse existing pattern).
- Many-to-one allowed (no enforcement), 1:1 expected. T3: single cachedT3 pointer, first-found-wins.
- Lantern + T3 are BOTH find-and-read consumers; same pairing mechanism, build ONCE.
Build AFTER T3 exists (T3 needs the pairing to find its Intertropical).

### 13. Rate discipline (Claude Code, see RATE_TABLE.md + RATE_DISCIPLINE_UNIFICATION.md)
WriteLedger infrastructure BUILT (SequencerEngine.hpp). Steps:
a) Wire noteWrite() at multi-writer sites: accentProb (x2 writers), restProb, scale mask.
b) Run debug build, watch conflicts fire, fix shape-A drifts.
c) Audit RATE_TABLE.md "Correct?" column vs running build.
d) Gate per-block reads behind sixteenthEdge where safe (NOT continuous ones: mix/spread/transpose).
e) Cap PPQN at 24 (only gs.tick uses sub-16th; higher PPQN = wasted per-pulse tick cost).
Lock mode Phase 2 (Causeway/Junction expander threading): AFTER this pass.

### 14. Scale additions (Claude Code -- small, see SCALES_AND_QUANTIZER_TODO.md)
Add to MonsoonScaleManager.cpp:
- Slendro (PRIORITY -- completes gamelan pair with Pelog; 12-TET approx {0,2,5,7,9})
- Chinese pentatonic (named/framing add for Singapore demographic completeness)
- Carnatic raga (completes Indian representation)
Label all honestly as 12-TET approximations. Rack listen to confirm they sound right.

### 15. External gate articulation check (Rack verify)

### 15b. Modes C & D (quantizer modes) pre-release pass -- NEGLECTED (see MODES_C_D_QUANTIZER_PRERELEASE.md)
Mode C (clock quantizer) + Mode D (gate2 quantizer) are real but long-neglected -- not re-examined as
engine/scale/PPQN/tuning changed around them (same risk as the Mode B regression). PRE-RELEASE, not
post-library: existing functionality that could ship broken. Verify in Rack (quantize external CV2 to
active scale, respect mask, live scale changes, PPQN-C interaction) + add standalone quantizer tests.
Also: when Monsoon Micro defines the tuning table (for A/B/E output), C/D MUST quantize to the SAME table,
not 12-TET -- design the tuning table as shared across ALL modes.
Confirm external-gate-driven steps route through the SAME rest/legato/accent-mode-B path as
internal clock, AND that Sands mods + big-5 modulation apply under external gate.
REQUIRED before publishing pitch doc point 3 claim. See EXTERNAL_GATE_ARTICULATION_CHECK.md.

---


### 16. Seed offset input + selectable scrub distance (Monsoon) -- MUST-HAVE
Rationale: we already allow forward AND backward navigation (dice-scrub, reversible, phase reverse),
so the inability to SET/OFFSET the counter position is an obvious gap -- you can move but not "go to".
The seed offset completes the navigation feature set and is the control surface the headline
"navigable probability space" claim has been missing. See CRAB_CANON_RECIPE.md.
- SEED OFFSET input: CV/param offsetting this Monsoon's Philox counter by a settable amount. Enables
  arbitrary canon alignment (offset one of two shared-seed Monsoons), the smooth phase-mirror crab
  canon, and generally makes navigable-probability a USER capability not just an internal property.
- SELECTABLE SCRUB DISTANCE (6/8/10/12 draws): sets how far a full dice-scrub traverses -> the crab
  canon's PERIOD becomes a compositional choice, independent of accumulated history. Param or context
  menu. Pairs with seed offset (offset = WHERE, scrub distance = HOW FAR).
Scope: touches PatternEngine (counter addressing) + Monsoon params/panel + reversible-mode scrub. Not
a library blocker, but high musical value and completes an already-shipped feature. Prioritise soon
after library (arguably a fast-follow, given it finishes the navigation story).

## PANEL ITERATION (after items 5, 8, 9 complete -- module set must be stable first)

T2/T3 must at least have final HP and jack counts before their panels are worth starting.
Raffles cleanup must be done (affects panel: fewer jacks, reflow).
Then: full panel pass across all 17 modules.

Known panel items:
- Intertropical: equatorial theme in. Needs eye on label crowding + font (DejaVu stand-in;
  optional Barlow swap when TTF available).
- Raffles: gates removed + panel reflow after trial cleanup.
- Changi T1: panel already has 16 jack markers (widget binds i<16); code just needs to match.
- T2: new panel (gen_changi_t2.py, reuse airport-terminal theme from T1).
- T3: new panel (gen_changi_t3.py, organised by channel, ~20HP).
- All modules: dark+light consistency, screws, brand lockup, label legibility.

---

## DEFERRED (post-library)

- Microtonal: Monsoon Micro 12-slider + 24-slider variants. Two FIXED panels (static art like
  current Monsoon, NOT widget-drawn reflow). ~10 classes in the 12-TET pipeline to generalise.
  See TWELVE_TET_AUDIT.md + SCALES_AND_QUANTIZER_TODO.md.
- Triplet step model: 1/16-triplet + 1/32 as real (non-snapped) subdivisions. Step-model feature;
  24 PPQN already carries them. Open design Q: global vs per-lane vs per-step ratchet.
- Quantizer modes: hard vs probabilistic-mask vs nudge (redistributeWeights).
- Monsoon Microtonal tuning ownership: self-contained Scala loader vs consume-upstream-tuning.

---

## PITCH + DOCUMENTATION (parallel track, not blocking code)

- PITCH_PATCHABILITY_AND_DISTINCTION.md: 8 legs complete. CODE-CHECK on point 3 (external gate
  articulation) needed before publishing. Otherwise ready for manual front matter + reviewer pitch.
- LIBRARY_SUBMISSION_CHECKLIST: item 2 above.
- Manual: Rodney's strength, deferred until stable enough to demo. Lead with the conceptual model
  (poly budget, three-space routing, lock mode musical meaning), Singapore naming thread, then the
  control reference.
- Demo patches: deferred until stable enough to play. Phase-drive + deep modulation is the headline
  demo. Targets: Omri Cohen (musical result in a video), CDM Peter Kirn (design/concept piece).

---

## KEY DESIGN DECISIONS ON RECORD (do not re-derive)

- Output-stage transpose: follows poly tie/legato (tie-latched, legato-live) but NOT Shophouse
  conservation. Deliberate performer escape hatch. See INTERTROPICAL_SPEC.md DECIDED note.
- CA domain-reverse: COMPARE BRANCH (decide in play -- item 7 above).
- Changi T3 data source: self-reading IT output jacks, NOT host-pushed. See CHANGI doc.
- PPQN cap at 24: musically correct (2^3 x 3, MIDI-standard). Not a compromise.
- Subgroup pairing: auto-numbered, immutable, emergent, source-binding only, clock orthogonal.
- Microtonal: POST-library, separate Monsoon Micro module(s).
- Reversibility: sampling + phase axes fully reversible; stateful transform-composition needs
  inverse-op care (the honest bound in pitch doc point 8).
- East/West textural continuum: correlation matrix spans homophony<->heterophony<->polyphony.
  Change Alley = content half; clocking = timing half. Scale-invariant principle (recurses from
  note level to arrangement level). See MULTIGROUP_CONSERVATION_AND_CORRELATION.md.
- Transpose in microtonal: restrict Intertropical transpose to OCTAVE-only (2:1, valid in any
  tuning; sidesteps the between-degrees problem cleanly).
- Convention: 1/16 is baked in (meloDICER inspiration); 1/8-triplet and 1/32 snap to 16th grid.
- Polymeter vs polyrhythm: POLYMETER (different lengths, same clock) is natively supported -- claim
  it confidently (Sands per-lane LOR, Intertropical per-scene, cross-Monsoon). POLYRHYTHM (different
  time bases) is also supported TODAY cross-instance: two Monsoons fed different gate inputs (e.g.
  1/16 straight + 1/16-triplet from the same master clock) = genuine 3:2 polyrhythm, each engine
  applying full generative articulation within its own time base. Phase inputs also work. Within one
  engine, the 1/16 grid is the current constraint -- the maybe-later triplet step model would add
  in-engine polyrhythm between lanes. Don't conflate the terms. See PITCH doc point 3.

---

## FUTURE: Shared CA + unified external seed across Monsoon instances

### The use case
Two Monsoons on different clocks (polyrhythm) sharing: (a) the same CA pin positions (correlation
structure), and (b) the same external seed (same probability space) = CORRELATED POLYRHYTHM. Already
partly achievable today (same external seed + simultaneous dice gate). CA sharing and a unified seed
mechanism make it first-class and patchable without manual coordination.

### Current CA architecture (facts from the code)
- CA V2 found by ADJACENCY POINTER WALK (reinterpret_cast typed pointer, same pattern as IT finding
  Monsoon). Not via expander messages.
- Monsoon reads CA fields DIRECTLY: rhythmSrc[N_VOICES], melodySrc[N_VOICES] via processDNA(),
  calls applyPendingTransforms() at the phrase boundary.
- EXTERNAL SEED for CA already flagged as TBD in the code (MonsoonChangeAlleyV2.hpp:56-57):
  "EXTERNAL-seed sharing is TBD (same open question as dice). The scatter draw builds a transient
  PhiloxRng from corrKey[ci]..." -- anticipated by the author. corrKey[] is the seed surface.

### Scope

#### Option A -- shared CA pin positions (second Monsoon reads same CA instance)
Changes: add "allow shared access" context-menu toggle on CA. Any Monsoon finding it can read
rhythmSrc/melodySrc. Only the OWNER Monsoon (the adjacent one) calls applyPendingTransforms() --
second Monsoon reads the already-applied state. Read-safety: rhythmSrc/melodySrc are written once
(CA process()) and read-only from both Monsoons -- safe. One caller of applyPendingTransforms only.
VERDICT: Small, bounded, well-understood. The shared-expander toggle is the main addition.

#### Option B -- CA external seed jack
Add SEED input jack to CA V2, feeding corrKey[]. Two CAs seeded identically start from the same
correlation-space position. A mult of one seed signal to Monsoon rhythm seed, Monsoon melody seed,
and CA seed = a unified seed for the whole correlated system (no new "seed splitter" module needed
-- just patch convention). CA seed jack alone is the buildable unit.
VERDICT: Small, self-contained. Pre-library candidate.

### Patching ideas (available today with existing features)
- Trigger DICE on both Monsoons simultaneously at polymetric RE-ALIGN (LCM point): a logic module
  detects re-align, fires gate to both dice inputs. Keeps probability spaces in sync at the musical
  downbeat of the polymetric cycle.
- ALTERNATING DICE: one Monsoon on odd re-aligns, the other on even. Engines drift and converge in
  probability space in alternation -- a meta-level polyrhythm of probability.
- CA SCATTER at re-align: reshuffles correlation structure of both Monsoons simultaneously (if they
  share a CA or both have scatter triggered). New correlation structure at every LCM point.
These ideas are worth capturing as demo patch concepts. The re-align detection is the interesting
patch problem (a counter/comparator watching both clock streams for their LCM point).

### Priority / library fit
- CA seed jack: PRE-LIBRARY candidate (small, completes the "unified seed" story, TBD already flagged).
- Shared CA pointer (Option A): POST-LIBRARY or alongside Changi T2/T3 work.
- Demo patch showing correlated polyrhythm: include in the demo batch when stable enough to play.

### Seed derivation (DECIDED: fixed key offsets, principled not a hack)
One external seed input derives THREE independent streams (rhythm, melody, CA) via fixed offsets on
the Philox KEY:
  rhythmKey = externalSeed
  melodyKey = externalSeed + 1
  caKey     = externalSeed + 2
WHY this is correct (not a shortcut): Philox's design guarantee is that streams with DIFFERENT KEYS
are statistically independent regardless of numerical proximity -- S+1 and S+2 are just as independent
as S+1000000. The fixed offset IS the standard mechanism for deriving multiple independent streams
from one seed, used in cryptographic and scientific RNG design. One seed -> three guaranteed-independent
streams -> deterministically reproducible from one value.
CRITICAL: offset must be on the KEY not the counter. Counter offsets produce correlated streams
(same-key sequences shifted in time -- they'd eventually generate the same values). Key offsets
produce genuinely independent streams. The seed feeds the KEY parameter.
USER MODEL: seed=42 -> same rhythmically, melodically, and correlationally every time. seed=43 ->
completely different but equally reproducible. Two instances with the same seed and simultaneous
dice gates = locked probability spaces. Clean, simple, correct.
ALTERNATIVE REJECTED: poly seed cable (3 channels = 3 independent values). Weaker -- requires
reproducing 3 values to reconstruct a patch; the fixed-offset single-seed approach is strictly better
(one value to reproduce everything, mathematical independence guaranteed).
