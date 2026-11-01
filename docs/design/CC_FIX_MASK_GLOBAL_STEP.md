# CC FIX — the output mask gates against the GLOBAL STEP (per the original spec), NOT voiceReadPosition

RESOLVED by going back to UNIFIED_ADDRESSING_SYSTEM.md. My prior fix docs were confused; this is the spec-
correct answer. Rodney's symptoms (Cycle ≡ Range, painted doesn't track) have ONE cause:
THE MASK IS GATING AGAINST THE VOICE'S READ POSITION, WHICH IS ALWAYS INSIDE ITS OWN WINDOW → NEVER CUTS.

## THE SPEC (UNIFIED_ADDRESSING_SYSTEM.md:331) — GLOBAL step, explicitly
"Zero the voice's GATE and CV OUT when the GLOBAL step is OUTSIDE the voice range (outside [O, O+L) —
GLOBAL-POSITION SPACE)." The mask gates against the GLOBAL playhead step, NOT the voice's read cell.
WHY: the global playhead travels all 16 steps; when the global step is outside [O,O+L) the voice is MUTED →
audible PER-VOICE SOUND REGIONS (the spec's payoff: entrances/exits, builds, voices occupying different
phrase regions). The reads are NOT special (they run as cycle-mode everywhere); ONLY the output is gated,
against the GLOBAL step.

## THE BUG — CC used voiceReadPosition (the voice's own read cell)
MonsoonOutputGenerator.cpp:52/127 pass engine.voiceReadPosition(vs) to outputMaskPasses. But the voice's
read cell is ALWAYS INSIDE its window (test_unified_addressing.cpp:550 kVoiceReads = {7..12,5,6,…} for
O=5,L=8 — every read ∈ [5,13)). So "global step outside window" NEVER fires → Range ≡ Cycle; painted cells
at out-of-read steps never hit → painted doesn't track. The STAGE4 test (outputMaskPasses(M,s)==(s>=5&&s<13))
pins the GATE FUNCTION as a pure function of s — CORRECT — but does NOT validate the CALLER's s; the caller
feeds the inert voice-read value. (My earlier "voiceReadPosition" fix doc was WRONG; the first "stepIndex"
instinct was right per spec — the confusion was letting the in-isolation test override the caller bug.)

## THE FIX
- MonsoonOutputGenerator: pass the GLOBAL step to outputMaskPasses for EVERY voice (mono + poly):
  outputMaskPasses(vs, engine.stepIndex) — the global-position step, per spec:331. NOT voiceReadPosition.
- outputMaskPasses UNCHANGED (its maskWindowContains(O,L,s) is correct — it wants the global-position s, and
  the STAGE4 pins already assume that space).
- Revert the two voiceReadPosition call sites (:52 mono, :127 poly) to engine.stepIndex.
- The phrase-wrap cut rule (spec:337-343) already lives in the per-step evaluation (gate carries across an
  in-range wrap; cut at the range's real end) — unchanged, just now actually exercised.

## VERIFY (Rack)
- Cycle = plays everywhere. Range = voice SILENT when the GLOBAL step is outside [O,O+L), AUDIBLE inside →
  visibly/audibly DIFFERENT from Cycle (a voice with window 5..12 goes silent for steps 1-4,13-16).
- Painted = the voice cuts at the painted-mute GLOBAL steps; the playhead visibly tracks the painted pattern
  (muted cells = silent). follow-R = the painted pattern rotates with R.
- Per voice: different windows/masks → different sound regions across voices (the arrangement-level payoff).
- Goldens: at Cycle default, outputMaskPasses is true every step → bit-identical. GOLDEN_A untouched.

## NOTE
The mask is an OUTPUT GATE in GLOBAL-position space — it carves per-voice SOUND REGIONS out of the phrase.
It is NOT keyed to the voice's read position (that's why it was inert — the read never leaves the window).
Spec:331 is explicit: GLOBAL step. The reads stay cycle-mode; only the output is gated, globally.
