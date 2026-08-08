# Lock mode Phase 2 — implementation plan

> Authoritative spec: [`LOCK_PHASE2_BUILD_SPEC.md`](../plugins/Melodicer/docs/design/LOCK_PHASE2_BUILD_SPEC.md:1)
> (written against current code; supersedes LOCK_MODE_RESUME's stale "INERT" claim). Reasoning:
> [`LOCK_MODE_AUDIT.md`](../plugins/Melodicer/docs/design/LOCK_MODE_AUDIT.md:1) (classification),
> [`LOCK_SEMANTICS.md`](../plugins/Melodicer/docs/design/LOCK_SEMANTICS.md:1) (ruling table).
> Where any older doc disagrees, the build spec wins.

## The model (Vermona-faithful, already the design)
Under LOCK: **params LATCH** (adjustments have no audible effect until unlock), **material keeps
generating** with pre-lock settings. LIVE controls ignore lock; QUEUE controls arm-and-fire at the
next phrase boundary. `liveNow(LATCH) == !locked`, so migrating an existing `if(!locked)` to
`liveNow(Control::X, locked)` is a behaviour-preserving no-op.

## Verified current state (audited Aug 2026)
- **Manager** [`MonsoonLockManager.hpp`](../plugins/Melodicer/src/dsp/managers/MonsoonLockManager.hpp:37):
  13-control `Control` enum; `categoryOf` + `liveNow` (instance + static forms) done. **QUEUE transport
  infra ALREADY EXISTS AND IS WIRED**: `tick(stepIndex)`, `boundaryNow()`, `unlockNow()`,
  `queueFires()` (lines 136-147), called at [`Monsoon.cpp:953-954`](../plugins/Melodicer/src/Monsoon.cpp:953)
  (`lockManager.tick(engine.stepIndex); expanderManager.sync(engine, lockManager.queueFires())`).
  The shadow-state absorption the spec worried about (`caV2PrevStep_`/`caV2PrevLocked_`) is DONE —
  the comment at :952 says it "replaces the old caV2PrevStep_/caV2PrevLocked_ shadow state".
- **Threaded LATCH (4/13)**: Spread (9 sites), Lor (6), ABMix (1: ModeController.cpp:65), Reseed
  (1: Monsoon.cpp:321). Confirmed via grep.
- **Calling-convention gap**: exactly ONE instance-style call —
  [`Monsoon.cpp:321`](../plugins/Melodicer/src/Monsoon.cpp:321) `lockManager.liveNow(Control::Reseed)`.
  All ~17 others use the static form `LockManager::liveNow(Control::X, engine.locked)`.
- **Direction/Owner**: NOT in the enum or `categoryOf` — must be ADDED (behaviour-changing: Direction
  doesn't latch today).

## Deviations from the spec's status table (code is ahead)
The spec table lists QUEUE as "Blocked on QUEUE mechanism (does not exist yet)". **The transport half
exists and is wired.** What's actually missing for QUEUE: the per-control ARM state (arm-on-gesture)
and threading Scatter to fire on `queueFires()` instead of writing continuously. Confirm at build how
`expanderManager.sync(..., queueFires())` currently consumes the flag for CA scatter.

---

## Build order (spec §"Build order + guard rails")

### Step 1 — Normalise calling convention [mechanical, behaviour-neutral]
Migrate [`Monsoon.cpp:321`](../plugins/Melodicer/src/Monsoon.cpp:321) from
`lockManager.liveNow(Control::Reseed)` → `dotModular::LockManager::liveNow(Control::Reseed, engine.locked)`.
Now all sites read identically (static form). No behaviour change.

### Step 2 — Add Direction + Owner to the enum [behaviour-neutral until threaded]
In [`MonsoonLockManager.hpp`](../plugins/Melodicer/src/dsp/managers/MonsoonLockManager.hpp:46), add to
the LATCH group of `Control` and to `categoryOf`'s LATCH set:
```cpp
Direction,   // per-lane traversal direction (editor.laneDir). Array READ, like LOR -> LATCH.
Owner,       // per-lane owner select. Twin with Direction -> LATCH.
```
Enum grows 13 → 15. Nothing calls them yet, so no behaviour change. Update any `NUM_CONTROLS`-sized
arrays if present (none found — `categoryOf` is a switch, `liveNow` a switch; safe).

### Step 3 — Thread the 4 pure-consolidation LATCH controls [behaviour-NEUTRAL]
`BigFive`, `NoteSliders`, `OctaveRange`, `Pins`. For each, find the sites that read the control into
the generation path (and the expander reads that map to it) and wrap the push with
`liveNow(Control::X, engine.locked)`. Per the audit, these sites already do `if(!locked)`, so this is
a rename — **tests must stay green, no audible change.** If behaviour changes, the audit missed a
site — stop and investigate.
- BigFive: the 5 rhythm params sampled into generation + Junction expander read.
- NoteSliders: 12 semitone weights → `semiWeights` (ONE gate, not 12) + Interchange writes.
- OctaveRange: OCT_LO/OCT_HI (separate gate from NoteSliders) + Interchange.
- Pins: CA pin matrix. NOTE: manual-pin-edit undo (StoreEditAction) is a SEPARATE path — don't
  conflate the lock gate with the undo gate.

### Step 4 — Thread Direction (+ Owner) [BEHAVIOUR CHANGE — Rack-verify]
Follow LOR's shape exactly (verified at [`MonsoonSandsManager.cpp:264`](../plugins/Melodicer/src/dsp/managers/MonsoonSandsManager.cpp:264)):
compute the value freely, gate only the **PUSH into engine traversal state** with
`liveNow(Control::Direction, engine.locked)`. The store write (`setLaneDir`) stays free so the UI still
moves under lock. Find the push site — the analogue of `engine.setStrand` — where `editor.laneDir` is
applied into the engine's traversal (laneDir push in the Sands visuals / sync path).
- **Owner check first**: the LOR comment says owner may already be latched "for free" (the LOR push
  gate freezes which base feeds the value). CHECK whether Owner needs a call site at all before adding
  one — it may be model-completeness only (like Transpose), shrinking this step.
- Acceptance (Rack): lock, change lane direction → control MOVES but traversal doesn't change; unlock
  → change commits. Should feel identical to LOR under lock.

### Step 5 — Verify LIVE controls have no stray gate [behaviour-neutral or a fix]
Clock, Mute, Display, Transpose = LIVE (never obey lock). Confirm no `if(!locked)` gates them today; if
one is found, REMOVE it (behaviour fix). Transpose: enum comment already asserts no gate — verify + close.

### Step 6 — QUEUE: arm Scatter through the existing transport [BEHAVIOUR CHANGE — Rack-verify]
The transport (`tick`/`queueFires`) already exists + is wired. Remaining: give Scatter an ARM state
(arm on the scatter gesture under lock; fire when `queueFires()`), rather than the phase-1 placeholder
where `liveNow(QUEUE) == !locked` (continuous). Confirm how `expanderManager.sync(engine, queueFires())`
consumes the flag for CA scatter today, then route the scatter commit through it.
- Acceptance (Rack): arm scatter under lock → fires at next phrase boundary (or unlock-flush), not
  immediately.

### Step 7 — Causeway
Confirm Causeway's need (LATCH via its expander read — audit says "Causeway → LATCH, modulates Straits
generation like Junction") is covered by step 3's threading. If it needs its own read gate not covered,
add it. Likely no new enum entry.

## Guard rails (spec)
- 30/30 tests green after EACH step.
- Steps 1, 2, 3, 5 = behaviour-NEUTRAL. Any audible change = a missed site; investigate before moving on.
- Steps 4 + 6 = behaviour-CHANGING by design; Rack-verify each specifically.
- LEAVE the engine's own freeze checks alone: `PatternEngine.cpp:176,286` `if(in.locked) return;` are
  audio-thread and must NOT round-trip the manager. Do not migrate them.

## Risks / to-resolve-at-build
- **(Q1) QUEUE consumption seam**: exactly how `sync(engine, queueFires())` currently uses the flag for
  CA scatter — is scatter already partly queued, or still continuous? Determines how much step 6 adds.
- **(Q2) Direction push site**: locate the single point where `editor.laneDir` becomes engine traversal
  state (the setStrand analogue). Direction is pushed from multiple Sands visuals — ensure ALL push
  sites are gated, not just one, or lock would be partial.
- **(Q3) Owner-for-free**: verify before adding an Owner call site (may be entry-only).
- **(Q4) BigFive/NoteSliders/OctaveRange site enumeration**: these currently have `if(!locked)` (per
  audit) OR no gate. If NO gate exists today, threading them is ALSO a behaviour change, not neutral —
  the spec assumes they already gate. Verify each has an existing `if(!locked)` before calling it
  "consolidation"; if not, reclassify that control's step as behaviour-changing + Rack-verify.

## Acceptance (whole phase)
- [ ] All 15 controls categorised; calling convention uniform (static).
- [ ] Direction + Owner in enum + LATCH set.
- [ ] BigFive/NoteSliders/OctaveRange/Pins threaded, tests green, no audible change.
- [ ] Direction latches under lock (Rack), commits at unlock — matches LOR feel.
- [ ] LIVE controls confirmed ungated.
- [ ] Scatter arms under lock, fires at boundary (Rack).
- [ ] Causeway covered.
- [ ] 30/30 tests green throughout.
