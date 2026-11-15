# CC PROMPT  triage EVERY literal 16 in the engine into THREE buckets (not a blanket replace)

Build on the current branch. The recurring failure this week: "16" means THREE different things in the engine, they look
identical, and replacing them uniformly breaks one while fixing another. Classify each occurrence; do not blanket-replace.

## THE THREE BUCKETS
1. POLYMETER PASS-THROUGH 16 (a LAW  KEEP, add a cap arm).
   The default-pattern raw-tick pass-through: `if (len == 16 && off == 0) return t;` and its driverTickBefore twin.
   A non-divisive lane free-runs (t-1)%len against the pattern  the drift IS the feature (Zoo adjudication; UA_NONDIV
   goldens). KEEP the 16 arm. ADD a cap arm so a full-ring (kPatternLengthCap) window also passes through
   (`|| (len == kPatternLengthCap && off == 0)`), congruent for every reader. Do NOT remove the 16 arm. Do NOT fold into
   a 16-walk.
   Sites: driverTickAt, driverTickBefore.

2. IDENTITY / TRANSPORT-DEFAULT 16 (WRONG at 32  change to kPatternLengthCap).
   Seeds and defaults that assume a 16-step pattern/window:
   - reset(): endStep = 15; cachedLength = 16;  (derive from the pattern length / cap, not literal 15/16).
   - reset(): lorRef(s, LOR_LEN) = 16;  (the identity window default  kPatternLengthCap; the "full 16-step window"
     comment is the vestigial framing).
   - voiceLorIdentity, the voice-row identity seeds, the reset-to-identity menus (bug 23).
   These take the cap (named constant, not 32).

3. VOICE-COUNT 16 (CORRECT  name it, do not change the value).
   `for (int i = 0; i < 16; i++)` over per-voice arrays (wasHeldPolyPrev, polyVerdict_, polyNv_, polyGhostOnset_,
   advanceOffset_, scanOfsV_, and similar). 16 = the voice count, NOT steps. Replace the literal with the named voice-count
   constant (kVoiceSlots / kMaxVoices  use the one already defined) so it reads as per-voice and can never be confused
   with a step 16. Value unchanged.

## TASK
1. grep every literal 16 (and 15 as a 16-derived bound) in SequencerEngine.cpp/.hpp and the lane/mask/driver path.
2. For EACH occurrence, assign bucket 1, 2 or 3 and act:
   - bucket 1: keep + add cap arm.
   - bucket 2: replace with kPatternLengthCap (or derive from the live pattern length where that is what it means).
   - bucket 3: replace with the named voice-count constant; value unchanged.
3. Produce a TABLE in the PR: file:line, the code, the bucket, the action. Any 16 that fits none of the three: STOP and
   ask; do not guess.

## GATES
- POLYMETER: UA_NONDIV goldens UNCHANGED; a len-7 lane under a 16 pattern still reads (t-1)%7; a full-ring window passes
  through identically (the new cap arm is congruent).
- IDENTITY: a reset gives a full kPatternLengthCap window; <=16 goldens bit-identical.
- VOICE-COUNT: no behaviour change; the loops cover kVoiceSlots voices as before.
- No literal 16 or 15 remains as a step/length/voice bound in the engine path except the polymeter-law 16 arm (which is
  commented as the law). Part of the magic-number audit (G).
- run_all green on both SDK legs; array-guard + anchor-audit clean.

## DO NOT
- Blanket-replace 16. Triage each.
- Remove the polymeter 16 arm.
- Change the voice-count value.
