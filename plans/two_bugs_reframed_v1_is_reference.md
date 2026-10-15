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

---

## CONSOLIDATION (Rodney) — melody/rhythm/qmix jitter are ONE bug, not three
The V2 melody jitter, the poly rhythm jitter, and the qmix jitter have the SAME symptom -> SAME root cause.
Not three bugs: ONE mechanism seen through three parallel streams.

### Why (structural)
Rhythm, melody, qmix go through IDENTICAL code paths (recomputeEffective{Rhythm,Melody,Qmix}[ForVoice] are
three copies of the same structure; all write random_[voice][STRAND_*] the same way; all subject to the
same two-writer interaction: control-rate recompute vs step-rate refreshPolyRandomCell applying
cachedPolySpread). So a bug in that SHARED per-voice write path manifests IDENTICALLY on all three streams,
for the same (poly) voices, V1-exempt (mono has no refreshPolyRandomCell second writer -> smooth).
The uniform-across-streams symptom is DIAGNOSTIC: it RULES OUT per-stream/per-lane index bugs (a melody-only
error wouldn't hit rhythm) and CONFIRMS the bug is in the shared per-voice machinery = the two-writer spread
race, V1 vs poly asymmetry. Exactly Bug B.

### Consequence — ONE fix, not three
Fix the shared two-writer race ONCE (reconcile the control-rate recompute with refreshPolyRandomCell so each
poly voice's random_[v] has ONE consistent writer, like V1/mono) -> ALL THREE lanes' poly jitter resolves
together.
CRISP TEST: after the fix, all three (melody, rhythm, qmix) go smooth TOGETHER on poly voices. If only one
goes smooth -> you touched a per-stream symptom, not the shared root; keep looking. "All three smooth or the
fix missed the root."
(Same pattern as the voice-2 bugs: parallel-stream structure means a SHARED-PATH bug shows as N identical
symptoms. Don't chase N; fix the one shared mechanism.)

---

## SHARPER Bug B diagnosis — "2 patterns flipping" = WRONG-VOICE INDEX, not a spread race (Rodney)
New symptom detail: the mix optimisation = engine computes at STEP BOUNDARY + controller view at VIDEO RATE
for the SELECTED voice only. It's NOT reaching the selected voice; what reaches FLIPS BETWEEN 2 PATTERNS of
probability.

"Flips between 2 PATTERNS" (not two near-values, not smooth noise) is diagnostic -> this is NOT the
spread-timing race I chased earlier. Two discrete PATTERNS = two writers putting DIFFERENT VOICES' data into
the display slot, alternating:
- Writer 1 (step-boundary engine): writes the CORRECT selected-voice pattern.
- Writer 2 (video-rate selected-voice view): writes a DIFFERENT voice's pattern because it's indexed to the
  WRONG voice ("not reaching the selected voice").
-> display alternates between the two voice patterns = the "2 patterns flipping".

So Bug B is most likely a WRONG-VOICE INDEX in the video-rate view (an off-by-one / wrong-base convention on
displayedVoice_ / selectedVoice), NOT a timing/spread reconciliation. It fits the convention debt exactly
(voiceIdx 0=V2 vs voiceSlot 0=V1 ambiguity on the display index). Supersedes the two-writer-spread-race
theory above for Bug B.

### Plan (Rodney): eradicate-then-log
1. Do the FULL mono-not-special / convention eradication (one base = voiceSlot 0=V1, everywhere incl the
   video-rate display-view index). This likely FIXES it — the video-rate view will hit the actual selected
   voice.
2. If it persists -> LOGGING: log (a) selectedVoice, (b) the voice index the video-rate view COMPUTES/WRITES,
   (c) the voice index the step-boundary engine wrote. The 2 flipping patterns = 2 different voice indices;
   the log shows WHICH is wrong in ~2 min. Then fix that one index.

NOTE the melody/rhythm/qmix consolidation still holds: if it IS a wrong-voice index in the SHARED video-rate
view path, all 3 lanes flip the same way (confirmed-same-symptom) -> one index fix resolves all 3.

---

## REFINEMENT (Rodney) — "flips between TWO patterns" = two different VOICES, not timing noise
New observation: the mix-opt design = engine computes at STEP boundary, controller view at VIDEO rate, only
for the SELECTED voice. Symptom: (a) it's NOT reaching the selected voice, and (b) what reaches jitter-FLIPS
between TWO patterns of probability.

### Why this is diagnostic
"Flips between TWO stable patterns" (not chaotic noise) means TWO complete, internally-coherent computations
are both landing in the display slot and the view alternates between them. Combined with "not reaching the
selected voice", the likely mechanism:
- the STEP-boundary engine path writes the CORRECT selected voice's pattern, AND
- the VIDEO-rate display path computes for the WRONG voice (off-by-one in "selected voice" index) -> a
  DIFFERENT voice's pattern.
-> the slot alternates between the right voice's and the wrong voice's patterns = "flips between two
patterns" = the SAME bug as "not reaching the selected voice". ONE bug: the video-rate displayed-voice path
targets the wrong voice INDEX (different base than the step-boundary path).

### This is the CONVENTION bug -> the mono-eradication should fix it
Two patterns = two voices = the two paths DISAGREE on what "selected voice N" means (different base). That's
the incomplete convention unification. So the full "mono is not special" / voiceSlot(0=V1)-everywhere
eradication should make both paths agree -> two patterns collapse to one (the right one) -> reaches the
selected voice. (vs the pure timing-race hypothesis, which would look like SMEARING/noise on ONE voice, not
two clean alternating patterns. The clean two-pattern flip points at TWO VOICES = convention, not timing.)

### If it persists after eradication — the ONE log line that decides it
For the selected voice, log: (a) the voiceIndex the STEP-boundary engine computes for, (b) the voiceIndex
the VIDEO-rate display path computes for, (c) whether the two probability patterns differ.
- indices DIFFER -> off-by-one/convention (should already be fixed by eradication; find the straggler site).
- indices SAME but values differ -> it IS the timing race (reconcile recompute vs refreshPolyRandomCell).
Bet: indices differ (two clean patterns = two voices). Plan: 100% mono-eradication first, then this log if
persistent.

---

## ✅ RESOLVED (commit 2b9d5ec5) — ROOT CAUSE = polyVoice() DOUBLE-CONVERSION (+1 applied twice)
Both bugs were ONE off-by-one. During the convention work, polyBankIndex was changed to return voiceSlot
(0=V1). But the bar display STILL added +1 (pv+1), a DOUBLE conversion:
  OLD: pv = polyVoice() [was 0-based poly]; vs = pv + 1   // correct when polyBankIndex returned 0-based
  NOW: polyBankIndex returns voiceSlot -> polyVoice() IS voiceSlot -> the +1 is one conversion too many.
Consequences (both predicted symptoms, now explained):
- BUG A (last voice zero): V16 -> voiceSlot 15, +1 -> index 16 = OUT OF BOUNDS on the [16] array -> reads
  zero/garbage. Last voice shows no data.
- BUG B (poly jitter "two patterns flip"): the display read the ADJACENT voice (off by one) -> the correct
  voice's pattern and the adjacent voice's pattern alternated in the slot = "flips between two patterns" =
  two voices, exactly the earlier refinement. V1 was uniquely smooth because index 0 doesn't overflow / the
  +1 bit the higher indices.
FIX: remove the UI +1; use polyVoice() (= voiceSlot) directly. One line. Also deleted the now-vestigial
MonoSandsParameterManager.hpp (170 lines) — one class-level suspect cleared.

### Validation of the whole diagnostic arc (every tell was right)
- "dual convention is the root" ✓ (two paths disagreed on whether +1 was already applied)
- "both bugs, one root" ✓ (one double +1)
- "two patterns = two adjacent voices" ✓
- "V1 is the reference / still special" ✓ (index 0 escapes the overflow)
- "verification was the bottleneck" ✓ — the fast+reliable test harness is what let it be TRACED to ground
  instead of guessed. The harness fix didn't fix the bug; it let the bug be SEEN.

### Still remaining (debt, NOT bugs)
The ~310-site full convention unification (voiceSlot everywhere) + the other vestigial classes (LaneMapping
+ SandsLaneNames, MonsoonSandsManager, identity-conversions, EL2ENG) remain as FUTURE-PROOFING cleanup —
they are no longer blocking behaviour. The convention work was far enough along that this ONE site fixed the
bugs. Verify in Rack: N=6/15 all voices show data (V16 nonzero); poly smooth incl melody+rhythm+qmix
together; V1 no longer uniquely smooth.
