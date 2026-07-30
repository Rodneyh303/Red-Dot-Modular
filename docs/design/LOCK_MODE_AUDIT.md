# Lock mode audit -- every lock-gate site classified (done with manager design in mind)

Classified each `if (!locked)` / `if (in.locked) return` site against LOCK_SEMANTICS.md §9 using
the READ-VS-MAP principle (shapes/reads probability arrays = LATCH; maps finished output = LIVE).

## Result summary
Every current site implements LATCH, and every one is CORRECT. There are NO wrong-category sites
and NO mis-implementations among the gated sites. The scattered `if (!locked)` checks are all
"skip the write to the engine arrays when locked" = the value latches = correct LATCH behaviour.

This is important for the manager design: the lock manager is NOT fixing broken behaviour. It is
CONSOLIDATING correct-but-scattered LATCH decisions into one place, and providing the category
model so FUTURE controls (and the OPEN-leaning ones) get classified once, centrally.

## Site-by-site classification

### Engine freeze (LATCH via total freeze) -- STAYS IN ENGINE
- PatternEngine.cpp:199  redrawRhythm       -> if(in.locked) return  [LATCH-freeze]
- PatternEngine.cpp:328  redrawMelody       -> if(in.locked) return  [LATCH-freeze]
- PatternEngine.cpp:396  applyPendingSeedsAndRedraw -> freeze seeds/RNG/patterns [LATCH-freeze]
  VERDICT: correct. These are the generation core. They MUST stay engine-side (audio thread,
  no manager round-trip). The lock manager does NOT absorb these -- it READS the same lock bool.
  The engine owns lock STATE + its own freeze; the manager owns the CATEGORY MODEL for controls.

### Monsoon.cpp
- Monsoon.cpp:319  reseed-on-restart gated by !locked  [LATCH]
  VERDICT: correct. Reseeding is generation; frozen under lock. Stays (engine-adjacent).

### Expander manager -- spread/prob writes into engine arrays (LATCH)
- ExpMgr:385  poly REST spread  -> engine write skipped when locked  [LATCH]
- ExpMgr:406  poly MELODY spread -> [LATCH]
- ExpMgr:434  poly OCTAVE spread -> [LATCH]
- ExpMgr:463  poly ACCENT spread -> [LATCH]
- ExpMgr:548  Macro V1 LOR/spread write -> [LATCH]
  VERDICT: all correct LATCH (spread = generation shaping, read-vs-map = READ = LATCH).
  These are the SITES THAT SHOULD BECOME MANAGER QUERIES: replace `if (!engine.locked)` with
  `if (lockMgr.liveNow(Control::Spread))` so the category (Spread=LATCH) lives in the manager.

### Expander manager -- CA transform apply (QUEUE + unlock-flush)
- ExpMgr:109  (vBoundary && !locked) || vUnlock -> applyPendingTransforms  [QUEUE]
  VERDICT: this is the QUEUE category (arm-and-fire-at-boundary) PLUS the unlock-flush. This is
  step b -- it becomes the lock manager's boundary/unlock EVENTS + queue ownership. The shadow
  state (caV2PrevStep_/caV2PrevLocked_) moves INTO the lock manager as legitimate business.

### Sands manager -- spread writes (LATCH)
- SandsMgr:325  mono/voice-1 spread gated  [LATCH]
- SandsMgr:452  SPREAD lock-gated (LOR above runs UNDER lock -- the LATCH/LIVE asymmetry!) [LATCH]
- SandsMgr:547  Macro global spread gated  [LATCH]
  VERDICT: correct LATCH. NOTE the documented asymmetry at :452 -- LOR runs under lock, spread
  doesn't. Per §9 both LOR and spread are LATCH; the "LOR runs under lock" is because LOR is
  seeded once (event-driven), not re-applied per frame, so there's nothing to gate. Spread IS
  re-applied per frame so it needs the gate. Same category, different application cadence.
  -> MANAGER QUERY candidates (lockMgr.liveNow(Control::Spread)).

### Mode controller -- A/B mix latch (LATCH)
- ModeController:63  latchMix gated by !locked  [LATCH]
  VERDICT: correct. §9 resolved A/B MIX = LATCH (upstream generation, pre-spread pre-pins; the
  "live crossfade" instinct was checked and rejected). -> MANAGER QUERY (Control::ABMix).

## What the lock manager owns (confirmed by the audit)
1. Lock STATE lives in the engine (engine.locked) -- unchanged. Engine keeps its own freeze
   checks (:199/:328/:396) -- LIVE-critical, audio-thread, no round-trip.
2. Lock MANAGER (new, Monsoon-side coordinator) owns:
   a. The control -> category MAP (the §9 table as DATA): Spread/LOR/ABMix/pins/direction... =
      LATCH; transpose = LIVE; clock/mute/display = LIVE; scatter = QUEUE. Single source of truth.
   b. liveNow(Control) query: call sites ask "should I apply this now?" instead of `if(!locked)`.
      Category logic lives here; call sites stop knowing WHY.
   c. Boundary + unlock EVENTS (absorbs ExpMgr's caV2PrevStep_/caV2PrevLocked_ shadow state) and
      the QUEUE for arm-and-fire-at-boundary controls (scatter). This is step b's proper home.
3. Engine-side freeze and manager-side category model READ THE SAME lock bool. No duplication of
   STATE; consolidation of DECISION.

## Migration shape (low-risk, because behaviour is already correct)
Because every gated site is already correct LATCH, migration is REFACTOR-ONLY for the LATCH set:
replace scattered `if (!engine.locked)` with `if (lockMgr.liveNow(Control::X))` where liveNow
returns !locked for LATCH controls. Behaviour identical; decision centralized. VERIFIABLE: tests
that currently pin lock behaviour (the ~23 assertions noted in §9) must stay green through the
refactor. THEN the OPEN-leaning controls (transpose=LIVE, direction=LATCH) and QUEUE (scatter)
get their real category, which is where behaviour may CHANGE (and where the inversion the doc
warns about actually lives -- but that's the transpose/direction rulings, not the already-correct
spread/LOR/mix LATCH set).

## Key insight
The doc's "lock mode inverts current behaviour" is NARROWER than it sounds. The spread/LOR/ABMix
LATCH behaviour is ALREADY correct and just needs centralizing (safe refactor). The inversion
applies to the specific OPEN rulings (transpose -> LIVE, direction -> LATCH) and making QUEUE
first-class. So lock mode splits cleanly into: (1) safe consolidation refactor of the correct
LATCH set into the manager, then (2) the genuinely behaviour-changing rulings on the smaller
OPEN/QUEUE set. Phase 1 is low-risk and delivers the manager; phase 2 is the careful part.

## LockManager scaling plan -- category-keyed, not per-physical-control

The phase-1 Control enum is a CATEGORY skeleton, not the full control inventory. Making lock
comprehensive means routing every physical control (knobs, sliders, CV) through liveNow() -- but
NOT giving every physical control its own enum entry.

### Granularity decision: category-keyed enum
Many physical controls share one category, so they share one Control value:
- Every spread knob/attenuverter across every manager/expander -> Control::Spread.
- Monsoon note-value + all light-sliders for notes -> Control::BigFive.
- Octave LO/HI + scale toggles -> Control::ScaleMask.
The enum stays SMALL (one entry per distinct category-behaviour); COVERAGE comes from threading
liveNow() through every control SITE, not from enumerating every control in the enum. A per-
physical-control enum (MonsoonNoteSlider_C, StraitsKnob_x, RafflesGate_3...) would explode to
hundreds of entries mostly mapping to the same handful of categories -- rejected.

The enum grows ONLY when a control needs a category that no existing entry expresses (e.g.
Transpose = LIVE, deferred to phase 2; Direction = its own LATCH ruling; per-expander entries
only where an expander's category differs from Spread/Lor/BigFive).

### Coverage status (what still needs threading + classification)
COVERED by existing enum entries (just need liveNow threaded through their sites):
- Monsoon note/octave light-sliders -> BigFive (notes) / ScaleMask (oct range). Category exists.
- Straits (East) LOR/spread -> Lor / Spread (§9: East inherits LATCH). Category exists.
- Big-5 sliders + CV mod -> BigFive. Category exists.

NEEDS its own enum entry (distinct category / OPEN ruling -- phase 2):
- Transpose -> LIVE (OPEN-leaning-LIVE, behaviour-changing).
- Lane Direction -> LATCH (OPEN-leaning-LATCH; read-vs-map = traversal = LATCH). Own entry.
- Mono/Macro owner -> OPEN-leaning-LIVE (structural routing). Own entry.

NEEDS per-expander CLASSIFICATION before an entry (audit §9 marked "confirm each"):
- Raffles / Interchange -> provisionally LATCH "if pure routing, revisit". CONFIRM: does each
  SHAPE generation (LATCH) or just ROUTE finished output (LIVE)?
- Junction (CV routing into big-5) -> provisionally LATCH (remote modulation path). Confirm.
- Causeway (poly rhythm CV) -> LATCH (poly REST/ACCENT mod = rhythm section). Likely fine.
- Changi -> LIVE if transport/vis (confirm role).
- Shophouse scale mask -> mask VALUES LATCH (like SEMI); the Conservation TOGGLE is orthogonal.

### Migration order (unchanged discipline: one group, tests green, commit)
Phase 1 (behaviour-preserving): thread liveNow(Control::Spread/Lor/BigFive/ScaleMask/ABMix)
through the already-correct LATCH sites. The ~23 lock assertions stay green throughout.
Phase 2 (behaviour-changing + inventory): Transpose/Direction/Owner rulings; per-expander
classification for Raffles/Junction/Interchange/Changi/Shophouse; QUEUE (scatter) arm-and-fire.

### Reality check (Rodney)
This is a LARGE surface -- every expander's controls, every Monsoon slider. The manager is the
right structure to absorb it, but comprehensiveness is incremental control-site work, not a
one-shot. The enum is the small stable core; the long tail is threading + per-expander
classification.

## Expander lock classification (Rodney's functional mapping + §9 confirmation)

Most expanders collapse to one or two categories by FUNCTION. Modulation inputs inherit the
category of the control they modulate (§9 modulation rule: CV of a latched control latches WITH
it). Classified:

### Junction -> Control::BigFive (LATCH), single category
All Junction modulation inputs feed the Big-5 rhythm knobs. CV of a LATCH target latches with it.
One category for the whole expander. liveNow(Control::BigFive) at its write sites.

### Interchange -> NoteSliders + OctaveRange (both LATCH)
All Interchange modulation inputs feed the 12 note light-sliders and the 2 octave sliders. Spans
TWO control groups, both LATCH. Functionally all-LATCH. If Interchange's inputs are distinguishable
(some -> notes, some -> octaves), query the matching group; if blanket, either works (same
category). CONFIRM: are the inputs separable note-vs-octave, or one blanket mod?

### Raffles -> ABMix (LATCH) + dice-gates (QUEUE) + slew (folds into QUEUE)
NOT uniformly one category -- but §9 already resolved it precisely (lines 228/288/300):
- A/B MIX inputs (RAFFLES_MIX_R/M_ATT/CV) -> Control::ABMix (LATCH).
- DICE / queued GATES -> QUEUE (arm-and-fire at boundary, like scatter). NOT LATCH.
- SLEW (RAFFLES_SLEW_R/M) -> folds into QUEUE, NOT an independent axis: it is SAMPLED at the
  phrase boundary (read at the queued redraw). So there is NO separate Control::Slew -- slew rides
  the queued roll. (Corrected an earlier wrong instinct to give slew its own LATCH entry.)
Raffles inputs ARE distinguishable (separate SLEW_R/M, MIX_R/M ids), so per-input-group queries
work: MIX -> ABMix, gates -> QUEUE, slew -> no independent query (consumed at queued redraw).

### Still to classify (unchanged from audit)
- Causeway -> LATCH (poly REST/ACCENT rhythm mod). Likely one category (like Junction).
- Changi -> LIVE if transport/vis (confirm role).
- Shophouse -> mask VALUES LATCH; Conservation TOGGLE orthogonal.
- Interchange note-vs-octave input separability (above).

### Pattern
Most expanders = ONE category (Junction=BigFive, Causeway=rhythm-LATCH). A few span two
(Interchange=NoteSliders+OctaveRange). Raffles is the outlier with three category behaviours
(ABMix LATCH / gates QUEUE / slew-into-QUEUE) because it touches mix + dice + slew. The category-
keyed enum handles all of this -- expanders just call liveNow() with the category their inputs
modulate; no per-expander enum entries needed except where a NEW category emerges (none did here).
