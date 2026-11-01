# CC DIRECTIVE — mask + voice-LOR work. THE AUTHORITY IS UNIFIED_ADDRESSING_SYSTEM.md (Rodney)

Supersedes the scattered mask debugging docs. Read UNIFIED_ADDRESSING_SYSTEM.md (the updated mask section +
the voice-LOR-gaps note at the end) as the SINGLE source of truth. The CC_FIX_MASK_* docs are DEBUGGING
HISTORY from a long session — do NOT build from them; some are superseded or were wrong.

## DOC STATUS (what to trust / ignore)
- UNIFIED_ADDRESSING_SYSTEM.md — AUTHORITATIVE (updated). Build from THIS.
- CC_FIX_MASK_GLOBAL_STEP.md — the "gate against the GLOBAL step" fix. LANDED (2a047dd). Correct principle;
  now part of the UAS doc. Reference only.
- CC_MASK_VS_VOICELOR_CONFLATION.md — the diagnosis (mask was using the read-confining voiceLOR). RESOLVED
  by CC_FIX_MASK_OWN_REGION + the UAS doc. Reference only.
- CC_FIX_MASK_OWN_REGION.md — the "gate uses its OWN region (maskPainted_), not effectiveVoiceLor" fix. This
  is the IMMEDIATE code change for the Range kind. Still valid; subordinate to the UAS doc.
- CC_FIX_MASK_VOICE_ADDRESSING.md, CC_FIX_MASK_PER_VOICE_STEP.md — DELETED (were WRONG, superseded).
- CC_PROMPT_UAS_MASK_UI_INCOMPLETE.md — the original UI spec (range-driven/paint/click-conflation). Its
  decisions are folded into the UAS doc; build to the UAS doc where they differ.

## THE WORK — in order. (A) FINISH THE MASK, then (B) VOICE-LOR PARITY.

### (A) FINISH THE MASK — per UNIFIED_ADDRESSING_SYSTEM.md (do first; it's in-flight/half-broken)
Build to the updated mask design:
1. The gate uses its OWN region (maskPainted_/maskAnchored_), NEVER effectiveVoiceLor. The READ runs
   full-phrase (voiceLOR unchanged); the gate silences output in ABSOLUTE phrase space against the GLOBAL
   step. (CC_FIX_MASK_OWN_REGION is the immediate change for the Range/MASK_DYNAMIC kind.)
2. Kinds: Cycle (all-on) / abs1 dynamic-from-the-GATE's-own-range / abs2 static painted / abs3 painted
   follows-R.
3. PAINT ONLY in abs2 (static): paint absolute cells at the voice's current BASE rotation (what you see =
   what you paint). You CANNOT paint in abs3.
4. ENABLING abs3 captures the current base rotation as the ANCHOR; the figure then slides (base-knob change
   re-anchors; CV rotation-mod offsets from the anchor — the doc's anchor mechanism). Re-edit by flipping
   back to abs2.
5. follows-R borrows the voice's R as a MODULATION SOURCE (figure slides with the voice's phase), NOT a
   confinement — the gate stays in absolute space independent of the read window.
6. Pins: Cycle default bit-identical; GOLDEN_A untouched. Verify in Rack: 4 kinds AUDIBLY differ, playhead
   traverses full phrase (not confined), per-voice masks give different sound regions.

### (B) VOICE-LOR PARITY — per the UAS doc's voice-LOR-gaps note (after A)
Bring voice LOR to full consistency with the probability-lane LORs. ON THE BASE PANEL, NEXT TO THE LANE,
same as every other lane (NO subpanel — that's a parked instrument-wide idea):
1. Add voice ROTATION (R): voice LOR has L and O but no R (inconsistent; it's also why the voice lane has no
   chevron — R denotes the chevron step). Add R knob + its mod jack + attenuverter, aligned to other lanes.
2. Add voice-LOR MODULATION: mod jacks + attenuverters for L/O/R, aligned to the other lanes. NO spread/warp
   (voice LOR doesn't need them).
3. DIRECTION + DELEGATION as BUTTONS/jacks next to the lane (currently context-menu only — move to buttons
   for parity).
All on the base panel. Reuse the existing per-lane control layout/widgets — do NOT invent new ones.

## PRINCIPLE (why this directive exists)
The mask debugging produced several contradictory docs; building from the wrong one wastes cycles. ONE
authority (the UAS doc); the CC_FIX_* docs are history. When in doubt, the UAS doc wins.
