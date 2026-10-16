# ⚠ CONVENTION UNIFICATION IS INCOMPLETE — root cause of the persistent voice bugs
Status after the collapse + "convention unification" commits: STORAGE collapsed to [16][16] (good), but
the VOICE-INDEX CONVENTION was NOT actually made singular. Two base conventions STILL COEXIST, often in the
SAME FILE. This is WHY the voice bugs (last-voice-zero, poly flicker) keep hiding and keep being
"thinks-fixed-but-isn't": every fix has to reason across two conventions, and the off-by-ones live exactly
at their boundary.

## Evidence (coexisting conventions, same files)
- MonsoonExpanderManager.cpp: `for (int v = 1; v <= effPolyVoices; v++)` (voiceSlot, 1-based, V1 implied at
  0) at L239 — BUT `for (int pv = 0; pv < effPolyVoices; ++pv)` (0-based, 0=V2) at L84/99/700. SAME FILE,
  BOTH bases.
- `voiceIdx` (0=V2 base) still widespread; `+1`/`-1` conversion sites still present (the hazard surface).
- `effPolyVoices = numPolyVoices` (EXCLUDES V1). Loops `v=1..effPolyVoices` cover V2..V(N) correctly, but
  any `pv=0..effPolyVoices` or display-read that assumes a different base is off by one at the boundary.

## Why this blocks the bug fixes
- BUG A (last voice zero): NOT the spread cache (that loop `v=1..effPolyVoices` DOES reach the last voice).
  It's elsewhere in the display READ path — a read index computed under the OTHER convention. Can't be
  pinned confidently while two conventions coexist; the "right" index depends on which base that site uses.
- BUG B (poly flicker, V1 uniquely smooth): V1 still has a separate `if (voice == 0)` branch in
  recomputeEffective*ForVoice; the poly path has a spread-TIMING race (recompute vs refreshPolyRandomCell
  apply cachedPolySpread at different times with different cross-voice neighbour state). Runtime timing +
  the lingering V1 special-case. Needs Rack observation + a singular path.

## The real remedy (bigger than more patches)
The incremental "fix one off-by-one" approach CANNOT converge while two conventions coexist — each fix is
correct under one base, wrong under the other, so bugs move rather than die (observed: 6+ failed attempts).
Options:
1. COMPLETE the convention unification FIRST, as a dedicated verified pass: pick ONE base (voiceSlot 0=V1)
   and mechanically convert EVERY 0-based `pv`/`dv`/`voiceIdx` loop and index to it, 0-based ONLY at the
   genuine cable-channel I/O boundary. Grep targets: `voiceIdx`, `pv = 0`, `dv = 0`, `+ 1`/`- 1` near
   voice, `effPolyVoices` used 0-based vs 1-based. VERIFY with a bit-compare harness that covers ALL voices
   incl the last, so an off-by-one is CAUGHT, not shipped.
2. THEN the two bugs become pinnable (single base -> the display-read index and the recompute path are
   unambiguous), and likely fall out.

## Process lesson (why it churned)
These are BEHAVIOURAL bugs (display/timing) with NO automated test catching "voice N shows data" or "poly
smooth like mono". So every fix was an UNVERIFIED guess — CC (and a blind edit) can't tell fixed from
not-fixed without Rack. Before more fixes: (a) complete the convention unification with a bit-compare
harness over ALL voices; (b) add a behavioural check (fill N voices -> assert all N nonzero). Then fixes are
verifiable instead of guesses. Until then, the fixes need Rack-in-hand (Rodney) — the bottleneck is
VERIFICATION, not typing.

---

## CLASS-LEVEL DEBT — vestigial bridges after unification (Rodney audit). The THIRD level.
"Mono/lane is not special" must be true at THREE levels, not two: STORAGE, INDEX/CONVENTION, and CLASS
STRUCTURE. The class level is UN-STARTED, and is likely WHY V1 still behaves specially (V1 may still route
through a mono-specific class/path that the array+index collapse never touched).

### Three-level eradication map
  dimension      | storage        | index/convention   | CLASS structure (NEW — not started)
  voice (mono)   | [16][16] ~95%  | voiceSlot ~10%     | MonoSandsParameterManager, MonsoonSandsManager,
                 |                |                    |   3rd ParameterManager  -> retire/merge
  lane           | order unified  | order unified      | LaneMapping(dsp) + SandsLaneNames(ui) + 14 lane
                 |                |                    |   identity-conv + EL2ENG  -> merge/delete

### Confirmed suspect classes (vestigial bridges — a representation unified but the bridge survived)
- **MonoSandsParameterManager.hpp** — mono-Sands retired; class persists. SMOKING GUN candidate for the
  V1-is-special bug: if V1 routes through this (its own param path) while poly doesn't, that IS why V1 is
  uniquely smooth. CHECK FIRST: is V1 handled by MonoSandsParameterManager and poly by a different manager?
  If so, retiring/merging it unifies the path -> likely fixes the jitter.
- **MonsoonSandsManager** — probable sibling/overlap with MonoSandsParameterManager (two Sands managers);
  merge.
- **3 ParameterManager classes** total — post-unification likely wants fewer (one per-voice param manager,
  not mono+poly+...). Check overlap.
- **LaneMapping.hpp (src/dsp/) + SandsLaneNames.hpp (src/ui/)** — TWO lane-reference systems in two layers
  (dsp + ui). Under one unified lane order they should be ONE canonical lane descriptor. Merge/minimize.
- **14 lane identity-conversion sites + EL2ENG** — bridges converting between now-IDENTICAL lane
  representations = dead. Delete (identity = no-op).

### NOT suspect (functional managers — leave): UIManager, ScaleManager, PersistenceManager, LockManager.

### The principle (unifies the whole cleanup)
Collapse to ONE REPRESENTATION per dimension (one voice model, one lane reference); DELETE every
bridge/manager/mapping/conversion that translates between representations that are now IDENTICAL. Vestigial
bridges don't just add dead weight — they PERPETUATE the split (keep the special-case scaffolding alive),
which is the likely home of the V1-special behaviour. So the "100% mono eradication" must include RETIRING
THE MONO CLASSES, not just the arrays/indices — and the lane cleanup must MERGE the two lane-naming systems
+ delete the identity conversions. Do all three levels or the split persists at whichever is left.

---

## THE LANE AXIS (found 2026-10-16, test_voice_conventions.cpp [C] — deliberately RED)

The voice-axis split above now has a LANE twin, caught statically by the Phase-0 convention
tests. SpreadInterp + SpreadManager + MonsoonSandsManager speak the OLD spread-lane order
(0=REST, 1=MEL, 2=OCT, 3=ACC, 4=QMIX, 5=VAR, 6=LEG — SpreadInterp.hpp:55, getOriginalValue's
switch, MonsoonSandsManager's raw 0/1/2/3/4/5/6 applyMono calls), while the unified PL_* world
(MEL=0, OCT=1, QMIX=2, REST=3, ACC=4, VAR=5, LEG=6 — PatternEngine::PolyLane) calls SpreadInterp
DIRECTLY from:
- PatternEngine.cpp recompute/promote sites (applyPoly with PL_REST/PL_MELODY/PL_OCTAVE/PL_QMIX/PL_ACCENT)
- SequencerEngine.cpp refreshPolyRandomCell (applyPoly with engLane)
- MonsoonExpanderManager.cpp (applyMono with PL::PL_* — unified)
- StraitsEastSandsVisual.cpp bar fill (applyAnchorV1Only, unified lane loop)
- StraitsSandsMacroVisual.cpp macroOwnProbability (applyAnchorV1Only, engLane)

### Crossing map (unified lane passed → array actually read by SpreadInterp's switch)
```
MEL(0) → Rhythm    OCT(1) → Melody    QMIX(2) → Octave
REST(3) → Accent   ACC(4) → Qmix      VAR(5) → Variation (coincide)   LEG(6) → Legato (coincide)
```
5 of 7 lanes crossed. Own AND target read the wrong lane's arrays wherever spread != 0 on those
paths — including the AUDIO poly spread path (PatternEngine.cpp's promote loops). At spread = 0
it is invisible (interpolate returns original), which is why it survived this long.

This also finally explains the V16 melody-bar flicker's round-5 signature: the melody bars'
spread TARGET was monoSlewedPub(PL_MELODY=0) = V1's RHYTHM row — the one array the diagnostics
never pinned, and the one that churns under the repro's rhythm-mix modulation. The out row
churned while pubSlewedPolyMelody[0]/[15] sat bit-stable because the actual target wasn't the
melody row at all.

### The fix — LANDED 2026-10-16 (option (a); Rodney: "fix the code instead of the test")
SpreadInterp's six switches + v1caSrc now speak the unified PL_* order; MonsoonSandsManager's
applyMono sites, SpreadManager::getOriginalValue and SandsParameterManager's getInterpolatedValue
calls converted to PL_*. The spread-VALUE fetch pairing (spreadE(0,X) / sprForLane(X) / spv[X])
is kept EXACTLY as before — see the remaining items. Behavioural at spread != 0 (crossed reads
become correct — NOT bit-identical there; spread = 0 paths bit-identical). The [C] contract pins
in test_voice_conventions.cpp now guard the unified order (green). Rack verification pending
(the V16 melody-bar flicker should be gone — its mechanism was this crossing).

REMAINING (Phase-2 VALUE-store audit, NOT fixed): the spread-value fetch columns keep their
historical sources — the stores behind them (engine spread, Monsoon editor spread, global
spread) may themselves be order-mismatched vs the menu's unified EDITOR_LONG indexing. Also
setMacroSpread's numVoices loop over the 16-wide slot bank.
