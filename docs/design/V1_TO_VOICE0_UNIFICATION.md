# V1 -> voice-0 unification — remove the mono special-cases (PRIORITISED)

## Why now
V1 is still structurally "Mono" in the code (separate ownership, display, slot, mirror paths) when
post-consolidation it should be JUST voice 0 of the unified per-voice system. EVERY V1-only bug this
project has hit is this debt collecting interest:
- V1 spread not respecting East ownership (fixed tactically, fe409c1).
- V1 display divergence (mono reads).
- V1 spread needs Macro to be/have-been connected (the getMonoMacroOwn-doesn't-default-East bug).
These are ONE root: V1 uses separate mono-slot machinery (getMonoMacroOwn, kMonoSlot, tab1MonoMirror,
onMonoTab, getMono*Base) that assumes the old Mono module / Macro arbitration, instead of the poly path
(getMacroOwn, slot=voice) that works for V2+. Whack-a-mole on symptoms keeps failing; kill the CLASS.

## The unification
Make V1 = voice 0 of the unified per-voice system EVERYWHERE. Collapse the mono-special paths onto the
poly path (voice index 0).

### Grep targets (every V1/mono special-case to collapse)
- `getMonoMacroOwn` -> `getMacroOwn(0, ...)` (V1 ownership = poly voice-0 ownership). THIS is the active
  bug: getMacroOwn resolves East-owned without Macro (V2+ work); getMonoMacroOwn doesn't (V1 broken).
- `kMonoSlot` / `MonoSlot` / `slot==0 special` -> voice-0 slot via the poly addressing.
- `getMonoRestBase` / `getMonoAccentBase` / `getMonoQmixBase` / `getMono*Base` -> poly voice-0 base.
- `tab1MonoMirror` / `onMonoTab` / "mono tab" branches -> treat the V1 tab as voice 0 (no mirror/special).
- `getMonoOwner` / `monoOwnerId` / `eastV1Owner` (vs eastPolyOwner) -> unify to the poly owner resolution
  at voice 0.
- `finalRandomByStrand` (bank 0 / mono reads) in the V1 display path -> read voice-0 per-voice like V2+.

### Strategy
1. For EACH special-case: confirm the poly path's voice-0 behaviour is correct (V2+ work, so voice 0 of
   the same path should), then route V1 through it and DELETE the mono-special branch.
2. Bit-compare / A-B: V1 behaviour after == V1 behaviour expected (match a working voice's logic at
   index 0). Don't just make the bug go away — make V1 structurally identical to voice 0.
3. Watch the ones with genuine V1 semantics (V1 = the mono/reference voice in some contexts) — a FEW
   may legitimately differ (V1 as the correlation reference). Keep only those; collapse the rest.

## Immediate (before the full pass): the getMonoMacroOwn bug
Make getMonoMacroOwn resolve East-owned when East present + Macro absent (mirror getMacroOwn, which V2+
use and which works). That fixes V1-spread-needs-Macro now; the full unification then removes the whole
class so it can't recur.

## Expected payoff
Kills the entire V1-only bug class at once (ownership, display, spread, future ones). Stops the
whack-a-mole. The consolidation "killed the Mono MODULE" but left the Mono CONCEPT in V1's code paths;
this finishes the job — V1 becomes voice 0, full stop.

---

## DECISIONS (Rodney) — full migration to index 0, + active-voices-only optimisation

### Full migration: V1 -> INDEX 0. Discard old patches (no backward-compat).
Don't care about existing patches (pre-release). So:
- **V1 = index 0, poly voices = 1..15** (index = voice number − 1). Natural, uniform.
- **Collapse kMonoSlot / kMonoMacroOwnRow=15 ENTIRELY** — no special mono row; everything uses v*7+lane
  (v*6 for laneDir) with V1 at v=0. Remove eastV1Owner, onMonoTab branches, the mono-slot special-casing
  (not a row-15 shim — actually move V1 to 0 and delete the special path).
- **NO migration code** — old patches are discarded (they'd load with scrambled voices; we don't care).
  Best for long-term maintainability: nobody later needs to know "V1 is secretly at row 15."
Progress so far (steps 1-7): ownership resolution + direction unified, dead mono code removed, V1
editability unblocked (~−128 lines). STILL TO DO: actually move V1 15->0, collapse kMonoSlot (35),
onMonoTab (22), eastV1Owner (9).

### Active-voices-only optimisation (compute N not 16)
Only produce data for the N poly voices selected (no Straits -> N=1, index 0 only; else N = poly count).
~16/N x saving on per-voice work for small counts (meaningful given the hard-won perf).
**What makes it STRAIGHTFORWARD (Rodney's simplifying constraints):**
- **Existing channels NEVER change on add or reduce** — only the DELTA moves: add -> spin up the new
  voices' data; reduce -> stop computing dropped ones. Voices that stay are untouched -> no glitch risk.
- **Pending dice roll doesn't affect regen** — draws are counter-addressed (deterministic fn of the
  counter), so a newly-active voice generates from the current counter, independent of pending dice.
  Determinism/reversibility preserved: activating voice v at step 30 gives the SAME data as if it'd been
  active since step 1 (counter-addressed, not activation-time-dependent).
- **Poly count is NEVER modulatable** — not CV, not DAW-automatable (deliberate: it's a CONFIGURATION
  knob, not a performance control; automating voice count is niche and better done by muting/gating).
  So: only POLL THE KNOB (cheap per-block), no modulation/automation handling.
- **Change lands at the NEXT STEP boundary** (≤1/16 away) -> regen the delta voices then. One step is
  quick enough that NO pending indicator is needed.
So the whole optimisation = poll knob -> on change, at next step, spin up/down the delta voices (existing
untouched, counter-addressed regen) -> otherwise compute only 0..N-1. Composes cleanly with V1=index 0
(the per-voice loop becomes 0..N-1). No mid-step glitch, no modulation handling, no pending UI, no dice
interaction.
