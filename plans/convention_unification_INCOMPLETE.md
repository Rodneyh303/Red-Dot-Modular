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
