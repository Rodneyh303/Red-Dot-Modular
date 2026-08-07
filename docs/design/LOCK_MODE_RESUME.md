# Lock mode -- where we are / what's next (resume note)

Branch: feat/lock-mode (off master, which now has the merged undo suite). Everything below is
committed. This note is the "pick it up later" summary; detail lives in the referenced docs.

## Done this session
1. Merged undo suite to master (items 1,2,3,5 verified in Rack; item 4 deferred to scrub).
   Fast-forwarded master to master_deparam (de-param arc done). All aligned at a5659e1.
2. Lock mode AUDIT (LOCK_MODE_AUDIT.md): classified all ~15 lock-gate sites. KEY RESULT --
   every gated site already implements LATCH CORRECTLY. Lock manager is CONSOLIDATION of correct-
   but-scattered decisions, not a bug fix. "Inverts current behaviour" is narrow: only the OPEN
   rulings (transpose->LIVE, direction->LATCH) + making QUEUE first-class actually change behaviour.
[STALE AS OF AUG 2026 -- see LOCK_PHASE2_BUILD_SPEC.md for verified current state. Item 3 below says
the manager is INERT; it is NOT. liveNow() is threaded through 4 controls (Spread x9, Lor x6, Reseed x2,
ABMix x2). 9 controls remain unthreaded, and Direction/Owner have no enum entry at all. The rest of this
note remains accurate.]

3. LockManager FOUNDATION built (src/dsp/managers/MonsoonLockManager.hpp), wired as
   Monsoon::lockManager{engine.locked}. INERT so far -- nothing calls liveNow() yet. 30/30 green.
   - LockCategory {LATCH, LIVE, QUEUE}; category-keyed Control enum (NOT per-physical-control).
   - Control groups corrected to real hardware: BigFive (5 rhythm knobs), NoteSliders (12),
     OctaveRange (2) are SEPARATE entries. Spread/Lor/ABMix/Pins/Reseed LATCH; Clock/Mute/Display
     LIVE; Scatter QUEUE.
   - categoryOf() = §9 table as code; liveNow(Control) = the query replacing `if(!engine.locked)`.
4. Expander classification (Rodney's functional mapping, confirmed vs §9):
   - Junction -> BigFive (LATCH), one category.
   - Interchange -> NoteSliders + OctaveRange (both LATCH). CONFIRM inputs separable or blanket.
   - Raffles -> ABMix (LATCH) + dice-gates (QUEUE) + slew (folds into QUEUE, sampled at boundary,
     NOT an independent axis -- no Control::Slew).
   - No expander needed a NEW enum entry -- category-keyed design held.

## Architecture (settled)
- Lock STATE = engine.locked (single bool). Engine keeps its OWN freeze checks (PatternEngine
  redrawRhythm/Melody/applyPendingSeeds :199/:328/:396) -- audio-thread, no manager round-trip.
- Lock MANAGER owns the CATEGORY MODEL: control->category map, liveNow() query, and (phase 2)
  boundary/unlock EVENTS + QUEUE (absorbing MonsoonExpanderManager's caV2PrevStep_/caV2PrevLocked_
  shadow state -- that's "step b" from the CA cleanup).
- Enum granularity = per control GROUP (12 note sliders = one entry), not per knob, not per
  category. Coverage = threading liveNow() through control SITES, not enum size.

## NEXT STEP (phase 1 migration, behaviour-preserving)
Thread liveNow() through the already-correct LATCH sites, replacing `if (!engine.locked)` with
`if (lockManager.liveNow(Control::X))`. liveNow(LATCH)==!locked so behaviour is IDENTICAL. The ~23
lock-behaviour test assertions must stay GREEN throughout -- that's the safety net.
Worklist (from LOCK_MODE_AUDIT.md), do incrementally, tests after each, commit per group:
- Spread writes: ExpMgr :385/406/434/463/548; SandsMgr :325/452/547 -> liveNow(Control::Spread).
- A/B mix: ModeController :63 -> liveNow(Control::ABMix).
- (Monsoon.cpp:319 reseed -> liveNow(Control::Reseed).)
Then thread the note/octave slider + big-5 knob sites (find their engine-write sites first).

## PHASE 2 (later, behaviour-CHANGING -- separate careful pass)
- OPEN rulings that invert: Transpose -> LIVE (add Control::Transpose); Lane Direction -> LATCH
  (add entry); Mono/Macro owner -> leans LIVE (add entry).
- QUEUE made first-class: Scatter + Raffles dice-gates arm-and-fire; the boundary/unlock EVENTS
  move from ExpMgr shadow state into the lock manager (CA cleanup "step b"). This overlaps the CA
  transform-apply path (applyPendingTransforms) already isolated on master.
- Per-expander finish: Causeway (likely one LATCH), Changi (LIVE?), Shophouse (mask LATCH /
  toggle orthogonal), Interchange note-vs-octave separability.
- Lock SCOPE context menu (§7): whole-module default; section/per-lane later.

## Watch-outs
- SequencerEngine has its OWN separate `locked` bool (SequencerEngine.hpp:397) -- check relation
  before touching either.
- Lock mode touches the LIVE signal path (unlike undo = history only). Bigger risk. One group at
  a time, build-verify in Rack, same discipline as undo.

## Phase 1 completion note

Phase 1 (behaviour-preserving migration of scattered LATCH guards to LockManager::liveNow) is
DONE. Migrated:
- ExpMgr spread x5 (:386/407/435/464/549) -> liveNow(Control::Spread).
- SandsMgr spread x3 (:325/452/547) -> liveNow(Control::Spread).
- ModeController A/B mix (:63) -> liveNow(Control::ABMix).
- Monsoon reseed (:319) -> liveNow(Control::Reseed).
All static-form (pass the same engine.locked) except reseed (instance form in Monsoon). 30/30 green
throughout -- behaviour identical (liveNow(LATCH)==!locked).

### Big-5 / NoteSliders / OctaveRange: NOTHING TO MIGRATE (important finding)
These controls are NOT gated by scattered if(!locked) checks. They flow to the engine
UNCONDITIONALLY (e.g. Monsoon.cpp:933 engine.accentProb = ...; big-5/note/octave populate
PatternInput every control tick). Lock is enforced DOWNSTREAM by the ENGINE FREEZE
(PatternEngine::redrawRhythm/Melody/applyPendingSeeds: if(in.locked) return) -- the values keep
being written but the engine ignores them when locked because it doesn't redraw. That IS correct
LATCH behaviour, via engine-freeze, which we agreed STAYS in the engine (audio-thread, no manager
round-trip).

So their Control enum entries (BigFive/NoteSliders/OctaveRange) exist for MODEL COMPLETENESS and
phase-2 use (if any becomes independently lockable), but there is no phase-1 call-site swap for
them. Phase 1 is complete.

### Engine freeze sites: intentionally NOT migrated
PatternEngine.cpp :199/:328/:396 (if(in.locked) return) stay engine-side by design -- the manager
reads the same lock; it does not absorb the freeze. Correct per the architecture.

### Phase 2 (next, behaviour-CHANGING) unchanged
Boundary/unlock events + QUEUE first-class (CA scatter, ExpMgr :110 shadow-state removal); OPEN
rulings (transpose->LIVE, direction->LATCH, owner); per-expander finish; lock-scope menu.

## Phase 2 progress (this session) + next precise step

### Done this session (phase 2)
- KEYSTONE: boundary/unlock detection moved from ExpMgr shadow state (caV2PrevStep_/
  caV2PrevLocked_, removed) into LockManager::tick()/queueFires(). Monsoon ticks it before
  expander sync; sync() takes caQueueFires. Behaviour-equivalent. QUEUE now first-class.
- Transpose -> LIVE: already correct in code (applied at genPitchLive output time,
  PatternEngine.cpp:131, downstream of the redraw freeze -- changes transpose the frozen pattern
  live under lock). No gate to invert; added Control::Transpose entry for model completeness.

### NEXT precise step: Direction -> LATCH (GENUINE behaviour change, needs a gate)
Traced the commit point. Direction uses a pending->committed mechanism:
  SequencerEngine.cpp:170  laneDir_[l] = laneDirPending_[l];  (+ laneDirV_ per voice :171)
gated by laneFlipQuant (StepEdge) -- commits at step edge, NOT gated by lock. So a direction change
made UNDER LOCK currently still commits at the next step edge. That is NOT latch behaviour.
To make direction LATCH: gate the pending->committed transition on !locked (don't promote pending
direction while locked; it commits at unlock). This is the FIRST phase-2 item that is a real
behaviour change with a code edit (transpose + note/octave sliders were already-correct no-ops).
Care: interacts with laneFlipQuant timing (StepEdge/other quant modes) -- gate the promotion, not
the quant logic. Owner (twin, LATCH) likely has an analogous pending/commit or direct-store path --
check whether owner writes are already frozen (store-backed, may already latch via the freeze) or
need the same gating.

### Remaining phase 2 after direction/owner
- Owner -> LATCH (check its commit path; twin with direction).
- Expander threading: Causeway/Junction/Interchange/Shophouse -> their LATCH categories; Changi
  LIVE. Most already flow through spread/big-5 paths already migrated or engine-freeze-enforced.
- CA scatter ARM path explicit through QUEUE; Raffles dice-gates QUEUE.
- Lock-scope menu (§7).

## Direction DONE (this session) + Owner analysis (needs tracing)

### Direction -> LATCH: DONE
Both promotion paths gated on !locked: StepEdge (SequencerEngine.cpp ~173) and Phrase-quant
(~278). Direction change under lock stays pending, commits at first unlocked step edge/phrase wrap.
Committed locally (push pending -- see below). Clarified: engine.locked (SequencerEngine::locked,
toggled via Monsoon::locked alias) IS the live lock; line-124 locked=false is reset() init only.

### Owner -> LATCH: analysis, NOT yet done
Owner is STORE-BACKED (editor.macroOwn[]/monoOwner[]/varlegDeleg[], Monsoon.hpp:723-747), written
IMMEDIATELY by setters -- NO pending/commit staging like direction. So owner currently takes effect
live = LIVE, not LATCH. To make it LATCH (twins with direction), owner under lock must freeze.

Owner is READ into topoIn (topology input) in the managers (SandsMgr:103/384, ExpMgr:36/81/83/321/
342) -- it shapes routing/ownership that feeds the spread/generation path. OPEN QUESTION: is topoIn
consumed at REDRAW (which freezes under lock -> owner might ALREADY effectively latch) or applied
LIVE? Must trace topoIn consumption before deciding:
- If owner's effect flows only through already-lock-gated spread/topology paths (phase 1), it may
  already latch -- no new gate needed (like transpose/note-sliders were already-correct no-ops).
- If owner leaks through a live path, it needs gating -- but since it's a direct store with no
  pending buffer, gating means either adding a pending/commit layer (like direction) OR gating the
  READ (skip re-reading owner into topoIn while locked, hold the last-committed topology).
NEXT: trace where topoIn is consumed (redraw vs live) to pick the approach.

### PUSH PENDING
Container was reset; fresh clone has no git credentials. Direction commit is LOCAL only. Needs
push access re-established to land on origin/feat/lock-mode.

## Owner trace RESULT: owner rides the LOR path (live-under-lock) -- tension with LATCH ruling

Traced owner -> topoIn -> SandsTopology::build -> topo.owner(0,l) -> selects baseLen/baseOff/baseRot
(MonsoonSandsManager.cpp:177-196): owner decides WHICH LOR BASE a lane reads (Macro's delegated
global base vs Mono's own LOR). Those bases push to the engine at ~:245-251, NOT lock-gated, and
this is BEFORE the spread lock gate at :453.

KEY TENSION: owner is "which LOR base does this lane read." LOR is intentionally LIVE-UNDER-LOCK
(phase 1: LOR runs under lock, only SPREAD is gated -- LOR is seeded once/event-driven, not
re-derived). So owner's effect rides a live path. Making owner LATCH is therefore INCOHERENT with
LOR being live:
- If user changes owner under lock, the lane's base switches live (because LOR is live).
- Freezing ONLY the ownership selection while LOR stays live = half-frozen incoherent state (frozen
  "who owns" but live "what the owned base is").
- To make owner truly LATCH you'd have to ALSO freeze LOR under lock -- contradicting the LOR-live
  ruling.

So "owner twins direction" doesn't hold mechanically: direction is a genuinely staged pending value
(gated cleanly); owner is a routing selector over the live LOR path. They are NOT the same kind of
control w.r.t. lock.

DECISION NEEDED (Rodney): 
(a) Owner stays LIVE (accept it rides LOR which is live-under-lock -- coherent, no code change,
    contradicts the "twins direction" ruling but matches the LOR-live architecture); OR
(b) Owner LATCH + also freeze LOR under lock (makes owner+LOR both latch, coherent together, but
    reverses the deliberate LOR-live decision -- bigger change); OR
(c) Owner LATCH via freezing only the topo.owner READ (hold last-committed ownership while locked,
    let the frozen owner still select a live LOR base) -- freezes the routing choice but not the
    base value. Coherent-ish: "you can't re-delegate lanes under lock, but a delegated lane's base
    still tracks live." Middle ground.
Leaning (a) for coherence with LOR-live, OR (c) if you want owner changes specifically frozen.

## CORRECTION: LOR is LATCH, not live. Owner trace tension was a MISREADING.

Rodney correctly challenged my "LOR is live-under-lock" claim. Checked LOCK_SEMANTICS.md:
- Line 137 (RESOLVED §9 ruling table): "DNA LOR (18 + globals + interp) ... LATCH". Line 150:
  mono lorBase LATCH. LOR IS LATCH.
- The lines I misquoted as "LOR live": line 21 describes the CURRENT (WRONG) behaviour ("Current
  (wrong): ... params stay LIVE ... you can ride ... LOR" -- the BUG lock mode fixes). Line 50
  argues LOR should NOT be live ("audibly live under lock BREAKS 'prepare silently'"). Both are the
  case FOR latching LOR, not a ruling that it's live.

So there is NO LOR-live ruling. My owner-trace "tension" was built entirely on misreading the
description-of-the-bug as the intended design. DISSOLVED.

### Consequence: owner = LATCH, cleanly (Rodney's original ruling holds)
- LOR is LATCH (should freeze under lock; currently wrongly stays live = a bug lock mode fixes).
- Owner rides the LOR path -> owner is LATCH too. "Owner twins direction" holds mechanically after
  all. No fork, no half-frozen incoherence.
- This is effectively option (b) but NOT "reversing a decision" -- it's implementing the ACTUAL
  ruling. LOR was always meant to be LATCH.

### Corrected target (supersedes the phase-1 "LOR runs under lock" note)
The phase-1 note "LOR runs under lock, only spread gated" described the CURRENT BUGGY state, not the
target. TARGET: LOR latches, spread latches, owner latches. Spread's gate is already implemented;
LOR's and owner's are not yet. All three should be LATCH. So:
- NEXT: gate LOR application under lock (LATCH) -- the missing piece; then owner latches for free by
  riding the now-latched LOR path (or gate the owner read too for cleanliness).
- Re-check: does gating LOR interact with the "LOR seeded once/event-driven" behaviour? Trace where
  LOR is applied vs where it's a one-time seed before gating.
