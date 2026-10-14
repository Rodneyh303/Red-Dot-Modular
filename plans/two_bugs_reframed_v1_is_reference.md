# TWO REMAINING BUGS — reframed. Collapse unified STORAGE but not the rest. (Rodney)
Not growing chaos — the SAME two bugs, still unfixed after several CC attempts. Reframed so they're
tractable. Both are "the collapse unified voice STORAGE (V1=index 0) but left V1 on a SEPARATE path / left
counts short by one."

## BUG B — V1 is the ONLY voice with SMOOTH rhythm-mix modulation; V2-16 flicker. (KEY REFRAME)
V1 is NOT supposed to be special anymore — it's voice 0, same as everyone. The fact that V1 is UNIQUELY
SMOOTH means V1 STILL RUNS ON A SEPARATE PROCESSING PATH. V1's path is the CORRECT one (smooth = one
writer). The poly path flickers = two-writer race: control-rate recompute (publishSlewed /
recomputeEffective*ForVoice writes random_[v] directly) vs step-rate refreshPolyRandomCell (overwrites
random_[v][lane][step] applying cachedPolySpread) — PatternEngine.hpp:105/116. Mono random_[0] is immune
because it has NO per-step spread refresh.

### Fix: V1 IS THE WORKING REFERENCE — unify the path so ALL voices behave like V1
Do NOT treat this as "poly has a bug to patch." Treat it as "V1 is still special when it shouldn't be;
FINISH unifying the path." Approach:
1. Find where V1 (index 0) takes a DIFFERENT code path than V2-16 in the mix/recompute/refresh flow (the
   mono slewedRhythm[0]/rhythmRandom written directly, no spread-refresh — vs poly through
   refreshPolyRandomCell).
2. Make the POLY path do what V1's path does — ONE consistent writer of the displayed random_[v]. Options:
   (a) recompute writes the PRE-SPREAD value into the stage refreshPolyRandomCell reads, so refresh is the
       SOLE writer of final random_[v] (applies spread on top) — same single-writer shape as mono; OR
   (b) recompute applies cachedPolySpread[v] in its write so it MATCHES what refresh produces -> no
       disagreement.
3. Then V1 can go through the SAME unified path (it should, post-collapse) — nothing special.
TARGET: ALL voices smooth like V1 is now; V1 no longer uniquely smooth. Do NOT suppress spread (loses the
feature) — RECONCILE so both paths yield the same value.
Diagnostic shortcut: V1's path is known-good. DIFF V1's path vs the poly path; the discrepancy IS the bug.

## BUG A — last selected voice shows ZERO Sands prob. Off-by-one. (mechanical)
Arrays went [15] (poly, bounded by numPolyVoices which EXCLUDES V1) -> [16] (V1-inclusive, 0=V1). A
numPolyVoices bound on a V1-inclusive array is SHORT BY ONE -> the LAST selected voice falls outside ->
unpopulated -> zero. Fix = the ~57-site audit in plans/collapse_count_offbyone_audit.md: V1-inclusive bound
-> activeVoiceCount() (= numPolyVoices+1); genuinely poly-only -> leave. This bug is in the Sands
BAR-DISPLAY / spread-refresh path. NOT engine fill / tabs / prob-OUT (all confirmed correct).

## Verify (RACK — behavioural, suite won't catch)
- BUG B: modulate rhythm-mix -> ALL voices smooth (V1 AND poly); V1 NO LONGER uniquely smooth.
- BUG A: select N = 2, 6, 15; roll dice -> ALL N voices show nonzero Sands data (no zero on the Nth).
- One bug per commit. Bit-compare unaffected states. 3 platforms.

## Process note (Rodney)
These are the SAME two bugs, not new breakage. Several "thinks fixed but isn't" attempts = fixes aren't
being VERIFIED before claiming done. Fix ONE bug, VERIFY IN RACK, commit, then the next. V1-as-reference
(Bug B) + the count audit (Bug A) are both now concrete with a known-good to compare against.
