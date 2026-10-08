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
