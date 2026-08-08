# Undo item 4 -- dice undo (build spec)

The last outstanding item in the undo suite. Items 1 (direction), 2 (LOR), 3 (knobs), 5 (CA) are DONE
and merged. This closes the suite.

## Finding that changes the original scope (verified in code Aug 2026)

The UNDO_IMPLEMENTATION_ROADMAP / MASTER_PLAN scoped item 4 as "REVERSIBLE mode only; free-run
undefined". **That constraint no longer applies -- reversible mode was REMOVED when the scrub model
landed.** Evidence:
- `src/MonsoonWidget.cpp:111` -- `TrialButton::inert()` is a dead stub: `return false; // reversible
  mode removed (scrub model) -- no per-stream flag`.
- `src/dsp/engines/PatternEngine.hpp:363` -- `int64_t rhythmDrawCtr = 0, melodyDrawCtr = 0; // signed:
  can go negative on reverse`. Signed counters, any value valid.

Under the scrub model every stream position is addressable, so **dice undo applies universally**. No
mode gating, no "free-run undefined" branch. This makes item 4 SIMPLER than originally scoped, not
harder. Delete the mode-conditional from the roadmap when implementing.

## Why this is small: Philox counters make undo a scalar restore

The dice draw is fully determined by (key, counter):
- Key: `rhythmKey = S`, `melodyKey = S+1` (seed derivation; persisted).
- Counter: `rhythmDrawCtr` / `melodyDrawCtr`, signed int64.
- Philox is a keyed bijection, so `at(N-1)` returns the previous draw EXACTLY -- no reseeding, no
  stored history of the drawn pattern.

So undo = **restore the counter scalar** and the identical pattern regenerates deterministically.
This is exactly why CA undo works (see MonsoonChangeAlleyV2.hpp:50-58 comment: "the counter is the
addressable position, so counter-- rewinds exactly"). Same architecture, and dice is the SIMPLER case.

### Strictly a subset of what CA undo already does
| | CA undo (item 5, DONE) | Dice undo (item 4) |
|---|---|---|
| Counter payload | `scatterCounter[8]` array | 2 scalars (`rhythmDrawCtr`, `melodyDrawCtr`) |
| Table state | ALSO snapshots `rhythmSrc[]`/`melodySrc[]` (non-scatter verbs mutate tables) | **None** -- draw is entirely key+counter |
| Thread handoff | audio -> lock-free ring -> UI `history::Action` | same |
| Action class | `TransformUndoAction` (MonsoonChangeAlleyV2.hpp:842) | new, simpler analogue |

Dice undo needs no table snapshot at all. Copy the pattern, drop the table half.

## Implementation

### Where the dice advance happens
- `src/dsp/engines/PatternEngine.cpp:191` -- `if (!first) advanceRhythmDraw(rhythmDrawDir());`
- `src/dsp/engines/PatternEngine.cpp:291` -- `if (!first) advanceMelodyDraw(melodyDrawDir());`
- Setters: `PatternEngine.hpp:399-400` -- `advanceRhythmDraw(int dir) { rhythmDrawCtr += (dir<0?-1:+1); }`

These are the mutation points. The USER-FACING dice action (button press / dice gate) is what should
push an undo entry -- NOT every internal advance. Identify the user-gesture entry point in Monsoon.cpp
(dice button handler / dice gate rise) and snapshot around THAT, not around the low-level advance.
Critical: a per-advance undo entry would flood the history stack with one entry per step.

### Snapshot shape
```cpp
struct DiceUndoSnapshot {
    int64_t rhythmBefore, melodyBefore;
    int64_t rhythmAfter,  melodyAfter;
    // Which stream(s) the gesture actually moved -- a dice press may hit rhythm only,
    // melody only, or both. Restoring an untouched stream is harmless (before==after)
    // but recording it keeps the action self-describing.
};
```

### The action class
Mirror `TransformUndoAction` (MonsoonChangeAlleyV2.hpp:842-865). Skeleton:
```cpp
struct DiceUndoAction : rack::history::Action {
    int64_t moduleId = -1;
    int64_t rBefore = 0, mBefore = 0, rAfter = 0, mAfter = 0;

    void undo() override { applyCounters(rBefore, mBefore); }
    void redo() override { applyCounters(rAfter,  mAfter);  }

private:
    void applyCounters(int64_t r, int64_t m) {
        // Same module-id resolution discipline as StoreEditAction: resolve by id,
        // no-op if the module is gone (survives deletion / undo-of-delete).
        auto* mw = APP->scene->rack->getModule(moduleId);
        auto* mon = mw ? dynamic_cast<Monsoon*>(mw->module) : nullptr;
        if (!mon) return;
        mon->engine.pe.rhythmDrawCtr = r;
        mon->engine.pe.melodyDrawCtr = m;
        // Regenerate at the restored position. See Monsoon.cpp:121-127 for the existing
        // save-counter / seed / restore-counter / regenerate idiom -- reuse that path
        // rather than inventing a new regenerate call.
        mon->requestRegenerateAtCurrentCounters();   // name TBD, match existing API
    }
};
```

### Thread discipline (reuse, don't reinvent)
The counter mutation happens in the AUDIO thread; `history::Action` push must happen on the UI thread.
CA already solved this with a lock-free ring (`undoHead`/`undoTail`, acquire/release, drop-if-full).
**Reuse that ring pattern** -- either extend the existing CA ring's payload type, or add a parallel
small ring for dice snapshots on Monsoon. Do NOT call `APP->history->push()` from `process()`.

### Regeneration after restore
Restoring the counters is not enough on its own -- the engine caches applied state
(`rhythmCtrApplied` / `melodyCtrApplied`, PatternEngine.cpp:336-339 compare against `rhythmDrawCtr`).
Those comparisons should naturally detect the restored counter as a change and trigger regeneration.
VERIFY this in Rack: if the cached-applied check doesn't fire, an explicit regenerate call is needed.
The existing idiom at Monsoon.cpp:121-127 (save counter, seed, restore counter, `regenerateRhythmB()`
/ `regenerateMelodyB()`) shows how a forced regenerate at a given position is done.

## Build order

1. **Find the user-gesture dice entry point** in Monsoon.cpp (button handler + dice gate rise). Confirm
   it's ONE place, or enumerate all of them. This is the snapshot boundary.
2. **Add `DiceUndoSnapshot` + ring publication** at that boundary: capture before, let the dice
   happen, capture after, publish.
3. **Add `DiceUndoAction`** (UI thread drains the ring, constructs and pushes the action).
4. **Verify regeneration on restore** in Rack -- does undo actually change the audible pattern back?
   If the `*CtrApplied` cache check doesn't fire, wire an explicit regenerate.
5. **Rack-verify the gesture feel**: dice, dice, dice, then ctrl-Z three times -- does it walk back
   through exactly the previous patterns? Redo forward -- same patterns again?
6. **Edge cases to test**: undo across a reseed (the seed-float snapshot per R1/R2 reconstructs the
   exact key+position, so restore is correct even when a reseed occurred between dice and undo -- test
   this specifically); undo after module deletion + undo-of-delete (module-id resolution); undo while
   running vs stopped.

## SCOPE RULING (Rodney, Aug 2026): dice/roll undo ONLY -- no undo of reset or reseed

Item 4 undoes the DICE/ROLL gesture (an edit), NOT reset and NOT reseed-on-reset (performative
gestures). Precedent: VCV SEQ-2 does not support undo of reset either -- the sequencer convention is
that reset is a timing action, not an editable state change.

Verified in code (Monsoon.cpp handleRestart:310-345), reset does four distinct things:
1. Playhead position (stepIndex, totalStepsElapsed=0, resetLaneWalk). Pure position state.
2. Gate state (engine.gs.reset()). Transient audio-thread state; regenerates next step.
3. Reseed IF reseedOnRestart -- re-keys Philox. Patched-SEED path is reproducible from the seed float;
   UNPATCHED path uses rack::random::u64() full entropy, NOT reproducible from a seed float.
4. CA re-key -- same as (3), one layer out.

Why NOT undo reset/reseed:
- Reset is performative, not an edit. By the time a user would hit undo, the clock has moved on;
  restoring a stale playhead position mid-run is not a musically meaningful state. VCV SEQ-2 agrees.
- Reseed is bundled WITH the reset gesture. Undoing reseed cleanly would force a decision about what
  happens to the playhead reset it came with (restore both? just the keys? -- neither is obviously
  right), which is a sign reseed-undo is not a natural unit the way dice-undo is.
- The unpatched reseed path injects raw entropy (rack::random::u64()), which does not reduce to a
  seed-float snapshot -- undoing it would need a 64-bit key getter on PhiloxRng that doesn't exist.
  More primitive than dice undo warrants.

CONSEQUENCE FOR THE RESEED HAZARD (simplifies R2 below): since reseed itself is never undone, the only
remaining question is what happens if a reseed occurs BETWEEN a dice gesture and its undo. See R2.

## R1 RESOLVED (Rodney): snapshot the SEED FLOAT + counter, not the derived key

Dice/roll advances ONLY the counter -- it does NOT change the key (Monsoon.cpp:374-380: "MAIN dice =
plain roll: advance the draw stream, no reseed"). So for the normal case the counter alone suffices.

PhiloxRng stores its key internally (std::array<uint32_t,2> key via seed64) and exposes NO getter --
the key cannot be read back. But the engine retains rhythmSeedFloat / melodySeedFloat
(PatternEngine.hpp:295-296), the 0..10 value that deriveKey turns into the key. That float IS the
stream identity: seedRhythmPhilox(seedFloat) re-derives the exact same key deterministically.

Snapshot shape:
```cpp
struct DiceUndoSnapshot {
    float   rhythmSeedBefore, melodySeedBefore;   // seed floats (stream identity)
    int64_t rhythmCtrBefore,  melodyCtrBefore;    // counters (position in stream)
    float   rhythmSeedAfter,  melodySeedAfter;
    int64_t rhythmCtrAfter,   melodyCtrAfter;
};
```
Restore (order matters -- seedRhythmPhilox zeros the counter as a side effect, so set counter AFTER):
```cpp
engine.pe.seedRhythmPhilox(snap.rhythmSeedBefore);  // re-derives key, zeros counter
engine.pe.rhythmDrawCtr = snap.rhythmCtrBefore;     // then restore position
// same for melody
```
This is the save/restore-counter idiom already at Monsoon.cpp:121-127. No PhiloxRng key getter needed;
restore is built entirely on existing public engine calls (lowest-risk primitive).

VERIFY AT BUILD: capture the APPLIED seed float (the one in effect at the applyPendingSeedsAndRedraw
commit point), not a pending value -- mirror the applied-vs-pending discipline already used for counters.

## R2 (reseed crossing the undo boundary) -- resolved by R1's seed-float snapshot

Since dice never changes the key, the seed float is invariant across normal dice gestures -- it just
rides along while the counter does the work. Its ONLY job is the edge case: if a reseed-on-reset occurs
between a dice gesture and its undo, the key changed underneath the counter. Restoring the OLD seed
float re-derives the OLD key, and restoring the old counter positions within it -- so undo is correct
even across a reseed. This is the spec's earlier "Lean (b)" but for the narrower, now-precise reason:
the seed float guards against a reseed CROSSING the undo boundary, it is not part of the dice mechanism.

Note: because reseed itself is not undoable (scope ruling above), there is no ambiguity about undoing
"into" a reseed -- the dice undo simply reconstructs the exact (key, position) it recorded, regardless
of what reset/reseed did in between.

## Remaining open decisions (flag at build)

- **Granularity when both streams move.** One action covering both, or separate actions per stream?
  **Lean one action** -- a dice press is one user gesture, so one undo press should reverse it.
- **Does dice-scrub push undo entries too?** Scrubbing moves the counter continuously; per-position
  entries would flood history. **Lean: scrub pushes ONE entry for the whole scrub gesture** (capture
  on scrub start, publish on scrub end), same as a drag on a knob produces one undo entry, not one per
  pixel.

## Guard rails

- Braces balanced + 30/30 tests green after each step.
- No `APP->history->push()` from the audio thread -- ring handoff only.
- Module-id resolution (not raw pointers) in the action, matching StoreEditAction discipline.
- The counter is signed and may be negative -- do not assume non-negative anywhere in the snapshot or
  restore path.

## Cross-refs
- UNDO_IMPLEMENTATION_ROADMAP.md -- the original roadmap (item 4 mode-gating is now stale, see above).
- MonsoonChangeAlleyV2.hpp:842-865 -- `TransformUndoAction`, the pattern to mirror.
- MonsoonChangeAlleyV2.hpp:50-58 -- the counter-as-addressable-position comment explaining why this works.
- src/ui/StoreEditAction.hpp -- module-id resolution discipline.
- Monsoon.cpp:121-127 -- existing save/restore-counter + regenerate idiom.
- PatternEngine.cpp:191,291 -- the advance sites; PatternEngine.cpp:336-339 -- the applied-cache checks.
- CA_DICE_COUNTER_MODEL.md -- the counter model these all rest on.
