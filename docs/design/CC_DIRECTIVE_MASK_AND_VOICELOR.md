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

### (A) FINISH THE MASK — CORRECTED composable model (Rodney re-read; supersedes earlier mask fix docs)
THERE IS ONLY ONE RANGE: the voice LOR range. No separate "gate range". Two COMPOSABLE axes, NOT four
exclusive modes:

AXIS 1 — BASE MODE (how the voice relates to its LOR):
- CYCLE (default = as if no mask): the voice CYCLES WITHIN its voice LOR — reads and plays within the LOR
  window, exactly as with no gate. (BUG to fix: it currently cycles FULL-PHRASE; it must cycle within LOR.)
- ABSOLUTE-1: the read runs FULL PHRASE (steps 1..16) but OUTPUT is SILENCED when the GLOBAL step is OUTSIDE
  the voice LOR range [O, O+L). Audible only inside the LOR window → sound regions. The abs1 gate region IS
  the voice LOR (effectiveVoiceLor), NOT a separate maskPainted_ region. (This corrects the deleted
  CC_FIX_MASK_OWN_REGION, which wrongly said "never effectiveVoiceLor" — abs1 DOES use the LOR range.)

AXIS 2 — PAINTED GATE OVERLAY (an EXTRA gate that COMPOSES on top of EITHER base mode; NOT a playback mode):
- The painted pattern (maskPainted_/maskAnchored_) is an ADDITIONAL mute, ANDed with the base mode:
  - paint + CYCLE  → cycling within LOR, additionally silenced at painted-out steps.
  - paint + ABS1   → full-phrase read silent outside LOR, additionally silenced at painted-out steps.
- FOLLOWS ROTATION (the old abs3) is a SEPARATE toggle on the painted overlay: enabling it establishes the
  painted pattern's link to the voice rotation; PAINTING IS DISABLED while follows-rotation is ON (edit in
  static, enable to make it move — the anchor is captured on enable). Painting is NOT a playback mode.

So: base = {cycle-within-LOR | abs1-full-read-silent-outside-LOR}; overlay = optional painted gate
(static or follows-rotation), composed (ANDed) on top. The painted overlay uses maskPainted_; the abs1
region uses the voice LOR. All gating is OUTPUT-ONLY (zero gate+CV), against the GLOBAL step; the READ is
never confined by the painted overlay (cycle reads within LOR by definition; abs1 reads full-phrase).
PINS: cycle default (within LOR) bit-identical to no-gate; GOLDEN_A untouched. Rack-verify: cycle plays
within LOR; abs1 silent outside LOR (full-phrase read); painted overlay composes with both; follows-rotation
moves the painted figure and disables painting.

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
