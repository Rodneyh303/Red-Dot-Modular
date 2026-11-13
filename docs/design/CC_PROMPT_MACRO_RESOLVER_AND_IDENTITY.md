# CC PROMPT  Macro voice-loop resolver bypass (bugs 19-21) + identity sentinel 16->cap (bug 23)

Build on feat/phase-duals-gaps. Authority: LOR_EDITING_BUG_LIST.md bugs 19-23, UNIFIED_ADDRESSING_SYSTEM.md,
VoiceResolver.hpp. Patch compatibility is NOT a concern (not released). No migration.

## PART A  MACRO: route the voice-frame loop through VoiceResolver (bugs 19-21)
Symptom (Rack, Macro module): V1 cycles all steps and emits no gates; V2 cycles a small unrelated range; V3 is correct.
On the Macro module everything is GLOBAL: ONE shared window and ONE output mask for ALL voices.
Cause (located): the voice-frame loop SequencerEngine.cpp:876-890 uses a RAW int v and a literal branch
`const bool live = (v == 0) || (v >= 1 && v <= numPolyVoices);`. This is the one place not going through VoiceResolver,
so slot 0 and the low-index voices do not resolve the shared Macro window/mask the way V3 does. (The identity ring-fold
in this loop is the bug-17 fix and is CORRECT; do not touch it.)
Fix:
1. Replace the raw v==0 / v>=1 test with VoiceResolver::isMono / isPoly. Resolve each voice's window and mask through
   effectiveVoiceLor and outputMaskPasses exactly as the working voices (V3) do.
2. REMOVE the bypass; do not add a mono branch. Any mono-specific value comes from VoiceResolver::mono() / kMonoSlot,
   never a literal 0.
3. Audit the engine for other raw per-voice indexing in the read/gate path (v==0, a bare int v loop over voices, %16 on a
   voice index) and route each through VoiceResolver or justify it in the PR.

## PART B  identity window sentinel 16 -> kPatternLengthCap (bug 23)  TWO DIFFERENT 16s, DO NOT CONFLATE
The adjudication (Zoo) found that "16" means two different laws. Keep them separate:

(i) IDENTITY-WINDOW SENTINEL (the bug-23 change). The full VOICE WINDOW. Change these to kPatternLengthCap:
    - voiceLorIdentity: L==16 -> L==kPatternLengthCap (SequencerEngine.hpp:834).
    - reset menu items + the voice-row/engine identity defaults (StraitsEastSandsVisual.cpp:944,:981;
      StraitsSandsMacroVisual.cpp:585; Monsoon.cpp:92; Monsoon.hpp:1013,:1115).
    Reset sets kPatternLengthCap/0/0/Forward; menu text reads the cap ("... identity (32/0/0, fwd)").

(ii) RAW-TICK PASS-THROUGH (the POLYMETER law)  DO NOT CHANGE. SequencerEngine.cpp:102 and :115 (len==16 && off==0) is
    NOT the identity sentinel: it is the pre-unification pass-through that lets a NON-DIVISIVE lane FREE-RUN against the
    pattern  a len-7 lane under a 16-step pattern reads (t-1)%7 (polymeter drift, realigns after 112 steps), NOT
    ((t-1)%16)%7 (the 16-walk fold, which realigns every 16 and DESTROYS the polymeter). The pass-through must fire for
    BOTH the default pattern length (the 16-arm, kept as the raw-tick golden law) AND the cap. Keeping the 16-arm is the
    whole point: it preserves every default 16-step patch's polymeter phase for 7/5/6 lanes.
    => Leave the two pass-through checks firing for the default pattern length. Add the cap arm if needed, but do NOT
       remove the default-length arm, and do NOT fold non-divisive lanes into a 16-walk.

RESULT: the UA_NONDIV golden digest is PRESERVED (not re-captured); the 11 pins stay on the raw-tick law.

## GATES (both parts)
- MACRO SHARED WINDOW: set a Macro window (O=16, L=8); every follower voice v in [kMonoVoice..kLastVoice] reads the SAME
  window, voiceRead_ in [O, O+L), identical period, for V1..V16. V1/V2 FAIL on this branch before the fix (the test must
  reproduce that), pass after.
- MACRO SHARED MASK: paint a Macro mask with bits off; outputMaskPasses is identical for every follower at every global
  step; masked steps silent for ALL voices incl. slot 0; mask all-on -> every follower emits gates; all-off -> none.
- MONO SLOT NOT SPECIAL: slot 0 under Macro reads the Macro window and mask exactly as slots 1..15 (direct pin on the
  former :876-890 path).
- IDENTITY (window sentinel): a reset gives a full 32-step voice window; the identity check fires at L==kPatternLengthCap.
- POLYMETER PRESERVED (the adjudication): a len-7 lane under a 16-step pattern reads (t-1)%7, NOT ((t-1)%16)%7. The
  UA_NONDIV golden digest is UNCHANGED and the 11 raw-tick pins stay. <=16 goldens bit-identical  the pass-through keeps
  its default-length arm.
- No literal 16 remains as an identity/length sentinel in the voice-window path (grep).
- The tests must DRIVE MACRO FOLLOWERS THROUGH THE RESOLVER across all voices  the gap that let this regress.
- run_all green on both SDK legs; array-guard and anchor-audit clean.

## DO NOT
- Special-case any voice index on the Macro module.
- Touch the bug-17 identity ring-fold.
- Add patch migration.
- Change East's working behaviour.
