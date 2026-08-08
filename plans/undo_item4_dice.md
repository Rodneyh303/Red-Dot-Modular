# Undo Item 4 — Dice undo (implementation plan)

> Authoritative spec: [`UNDO_ITEM4_DICE_BUILD_SPEC.md`](../plugins/Melodicer/docs/design/UNDO_ITEM4_DICE_BUILD_SPEC.md:1)
> (written against current code; where it disagrees with `UNDO_IMPLEMENTATION_ROADMAP.md`, the build
> spec wins). Items 1/2/3/5 are DONE + merged; this closes the undo suite.
> Pattern to mirror: CA's `TransformUndoAction` + lock-free ring (item 5, done).

---

## 0. Verified code state (audited this session, not just doc-trusted)

- **Dice gesture = deferred + mode-free.** `Monsoon::diceRhythm()`/`diceMelody()`
  ([`Monsoon.cpp:374,381`](../plugins/Melodicer/src/Monsoon.cpp:374)) and `fireDieAction()` (:388) only
  set PENDING flags (`setPendingRhythmRoll()` etc). No reversible-mode gate remains — confirmed the
  spec's "applies universally" finding.
- **The single commit point** is `PatternEngine::applyPendingSeedsAndRedraw(in)`
  ([`PatternEngine.cpp:352`](../plugins/Melodicer/src/dsp/engines/PatternEngine.cpp:352)), called from
  `onPhraseBoundary` → `Monsoon::onPhraseBoundary_()`. It consumes the pending flags and calls
  `redrawRhythm`/`redrawMelody`, which do `advanceRhythmDraw(...)` (:191/:291) — THE counter mutation.
  This is where before/after must be captured (audio thread).
- **Counters**: `int64_t rhythmDrawCtr, melodyDrawCtr` ([`PatternEngine.hpp:363`](../plugins/Melodicer/src/dsp/engines/PatternEngine.hpp:363)),
  signed, negative legal. Keys via `seedRhythmPhilox`/`seedMelodyPhilox` (today's `deriveKey`).
- **Regeneration on restore is AUTOMATIC**: `recomputeEffectiveRhythm()` is gated by
  `rhythmDrawCtr != rhythmCtrApplied` ([`PatternEngine.cpp:336`](../plugins/Melodicer/src/dsp/engines/PatternEngine.cpp:336)).
  Setting the counter back makes the next control-rate pass detect the change and regenerate. **No
  explicit regenerate call needed** — but VERIFY in Rack (spec step 4).
- **Reseed zeroes + re-keys** ([`PatternEngine.cpp:362-366`](../plugins/Melodicer/src/dsp/engines/PatternEngine.cpp:362)):
  a bare counter from before a reseed points into a DIFFERENT stream. Confirms the spec's reseed
  hazard → record `(key, counter)` (Lean b).
- **CA ring to mirror**: `undoRing`/`undoHead`/`undoTail` + `TransformUndoSnapshot` +
  `TransformUndoAction` + widget `step()` drain ([`MonsoonChangeAlleyV2.hpp:75-93, 861-910`](../plugins/Melodicer/src/MonsoonChangeAlleyV2.hpp:75)).
  Dice is a strict subset: counters+keys only, NO table snapshot.

---

## 1. Decisions — RESOLVED by Rodney (spec updated Aug 2026)

1. **Scope: dice/roll undo ONLY.** No undo of reset, no undo of reseed-on-reset (performative
   gestures, not edits). Precedent: VCV SEQ does not undo reset. (Spec §"SCOPE RULING".)
2. **R1 RESOLVED: snapshot the SEED FLOAT + counter**, not the derived key. `PhiloxRng` exposes no key
   getter, but the engine keeps `rhythmSeedFloat`/`melodySeedFloat` (the 0..10 stream identity), and
   `seedRhythmPhilox(float)` re-derives the exact key. Restore = `seedRhythmPhilox(seedBefore)` then
   set `rhythmDrawCtr = ctrBefore` (ORDER MATTERS: seed zeros the counter, so set counter AFTER). Built
   entirely on existing public engine calls — lowest-risk primitive, no new PhiloxRng API.
3. **R2 RESOLVED by R1**: dice never changes the key, so the seed float is invariant across normal
   dice; its only job is the edge case where a reseed-on-reset crosses the undo boundary — restoring
   the old seed float re-derives the old key, so undo is still correct. No barrier needed.
4. **One action per dice gesture covering both streams** (spec Lean). A dice press = one Ctrl+Z.
5. **No mode gating** — deleted; universal under the scrub model.
6. **Scrub**: lean one-entry-per-scrub-gesture IF scrub is a wired counter-moving entry point; confirm
   at build (may be out of item-4 scope if scrub isn't a distinct gesture yet).

---

## 2. The design — capture at the commit, key it to the gesture

The subtlety the spec flags: the commit (`applyPendingSeedsAndRedraw`) runs at the phrase boundary and
can fire for REALTIME-mode redraws (`rhythmMode==1`) too, not only user dice. We must record an undo
entry ONLY for a **user dice gesture**, else the history floods with one entry per phrase in realtime
mode. So:

- **Arm on gesture**: when `diceRhythm/diceMelody/fireDieAction` set a pending ROLL from a user action,
  also set a transient `rhythmDiceUndoArmed` / `melodyDiceUndoArmed` flag on the engine.
  (Distinguishes user roll from realtime-mode auto-redraw, which does NOT arm.)
- **Capture at commit**: in `applyPendingSeedsAndRedraw`, for each stream, if its undo-armed flag is
  set: record `before = {key, ctr}`, let the redraw run, record `after = {key, ctr}`, publish ONE
  combined snapshot to the ring, clear the armed flags.
- **Publish**: a Monsoon-side (or engine-side) lock-free ring, mirroring CA. Drained on the UI thread
  in `MonsoonWidget::step()`.

### Snapshot shape
```cpp
struct DiceUndoSnapshot {
    bool     movedR = false, movedM = false;   // which streams this gesture actually rolled
    uint64_t keyRbefore, keyMbefore, keyRafter, keyMafter;
    int64_t  ctrRbefore, ctrMbefore, ctrRafter, ctrMafter;
};
```
(Key before/after differ only if the gesture reseeded — main dice does NOT reseed under the scrub
model ([`Monsoon.cpp:375-377`](../plugins/Melodicer/src/Monsoon.cpp:375)), so key is usually stable;
recording it is the cheap insurance the spec wants.)

### The action
```cpp
struct DiceUndoAction : rack::history::Action {
    int64_t moduleId = -1;
    DiceUndoSnapshot s;
    void undo() override { apply(/*before*/true); }
    void redo() override { apply(/*before*/false); }
    void apply(bool before) {
        auto* mon = resolveMonsoonById(moduleId);   // id-resolution like StoreEditAction
        if (!mon) return;
        auto& pe = mon->engine.pe;
        if (s.movedR) { pe.setRhythmKeyAndCounter(before ? s.keyRbefore : s.keyRafter,
                                                   before ? s.ctrRbefore : s.ctrRafter); }
        if (s.movedM) { pe.setMelodyKeyAndCounter(before ? s.keyMbefore : s.keyMafter,
                                                   before ? s.ctrMbefore : s.ctrMafter); }
        // recomputeEffective* fires automatically via the ctr!=ctrApplied gate (verify in Rack).
    }
};
```
Need small engine setters `setRhythmKeyAndCounter(key,ctr)` / melody that set the Philox key directly
(NOT via `seedRhythmPhilox`, which would zero the counter) + set the counter. Add these to
PatternEngine — they're the restore primitive.

---

## 3. Build order (mirrors spec §"Build order", each step tests green)

1. **Engine restore primitives**: `PatternEngine::setRhythmKeyAndCounter(uint64_t,int64_t)` +
   melody. Set `rhythmPhilox.seed64FromKey(key)` (or store the raw key — check PhiloxRng API: `seed64`
   conditions the key, so we need the SAME conditioning both ways; simplest is to record the SEED
   FLOAT or the post-conditioning key consistently). **Resolve the key representation first** — see
   §4 risk. Then set `rhythmDrawCtr = ctr`. No redraw here; the applied-cache gate handles it.
2. **Arm flags**: add `rhythmDiceUndoArmed`/`melodyDiceUndoArmed` to PatternEngine; set them in the
   user-gesture setters used by `diceRhythm`/`diceMelody`/`fireDieAction` (the ROLL + LastRoll paths).
   Do NOT arm on realtime-mode auto-redraw or on seed-from-SEED-input.
3. **Capture + ring**: in `applyPendingSeedsAndRedraw`, wrap each stream's redraw with the
   before/after capture gated on the armed flag; publish one `DiceUndoSnapshot` to a lock-free ring
   (mirror CA's `undoHead`/`undoTail` acquire/release + drop-if-full).
4. **Action + drain**: add `DiceUndoAction`; drain the ring in `MonsoonWidget::step()` (UI thread),
   push one action per snapshot. (CA does this in the CA widget; dice ring lives on Monsoon, drained
   in MonsoonWidget.)
5. **Rack verify regeneration**: dice ×3, Ctrl+Z ×3 — does the audible pattern walk back exactly?
   Redo forward — same patterns? If the `*CtrApplied` gate somehow doesn't fire, add an explicit
   `regenerateRhythmB()`/`regenerateMelodyB()` in the restore.
6. **Edge cases**: undo across a RESET-reseed (key differs → the recorded key makes restore correct,
   OR we barrier the history at reseed — decide per §4); undo after module delete + undo-of-delete
   (id resolution); undo while running vs stopped.

---

## 4. Risks / open items to resolve AT build (not guess)

- **(R1 — highest) Key representation round-trip.** `seedRhythmPhilox(float)` conditions the float into
  the Philox key and zeros the counter. For undo we must restore the key WITHOUT zeroing the counter,
  and the recorded "key" must reproduce the exact same stream. Options: record the **seed float**
  (`rhythmSeedFloat`) + counter and restore via a new "set key from float, keep counter" setter; OR
  record the conditioned 64-bit key and expose a raw `PhiloxRng` key set. **Confirm PhiloxRng's API**
  (`seed64` conditions; is there a raw-key path?) before writing the setter. Pick the representation
  that guarantees byte-identical regeneration.
- **(R2) Realtime-mode flood.** Must confirm the armed-flag gate fully excludes `rhythmMode==1`
  auto-redraws. If realtime redraw shares the exact pending path, the flag is the discriminator — test
  that toggling realtime mode produces NO dice-undo entries.
- **(R3) `fireDieAction` sources.** Dice can fire from the panel button, G3 menu-routed gate, and
  Raffles gates ([`Monsoon.cpp:388`](../plugins/Melodicer/src/Monsoon.cpp:388)). All route through
  `diceRhythm/diceMelody`, so arming there covers all sources — verify no other direct
  `setPendingRhythmRoll` call bypasses the arm.
- **(R4) LastRoll / reverse.** `setPendingRhythmLastRoll` inverts the dice dir this boundary; the
  counter still moves by ±1, so before/after capture is direction-agnostic. Confirm.
- **(R5) Both-streams-in-one-gesture vs separate presets.** Panel has separate R and M dice buttons,
  so a single press usually moves ONE stream. The one-action design still holds (movedR/movedM flags);
  a combined "dice both" gesture (if any) produces one action moving both.

---

## 5. Thread + resolution guard rails (from spec)
- NO `APP->history->push()` from `process()` / audio — ring handoff only.
- Module-id resolution in the action (survives delete / undo-of-delete), like `StoreEditAction`.
- Counter is signed; never assume ≥0 in snapshot or restore.
- Braces balanced + 30/30 tests green after each step.

## 6. Acceptance
- [ ] Dice R/M each push exactly one undo entry per press (not per phrase, not per advance).
- [ ] Ctrl+Z restores the previous pattern audibly + in the Lantern; redo re-applies.
- [ ] Realtime mode produces NO dice-undo entries.
- [ ] Undo correct across a reset-reseed (recorded key restores the right stream).
- [ ] Survives module delete + undo-of-delete (id resolution).
- [ ] 30/30 engine tests green; optionally a new test asserting counter round-trips a roll.
