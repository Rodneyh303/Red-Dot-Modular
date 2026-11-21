# CC PROMPT — 32 STEPS, STAGE 1: structural (length 1..32; ≤16 BIT-IDENTICAL) (Rodney-settled)

Authority: STEPS_32_PLAN.md (settled decisions + build plan). This is STAGE 1 ONLY. Do not start Stages 2-5.
Branch: off lane-expander-refactor (position-holders, UAS, rhythmPattern-delete, DNA_LCM-kill all merged).

## GOAL
Pattern / phrase length accepts 1..32. DEFAULT and every existing patch stays 16. The ≤16 golden battery
must be BIT-IDENTICAL for every ≤16 configuration. This is the regression gate for the whole 32-step feature.
If ANY ≤16 golden moves, STOP — find the cause before proceeding. Do not re-pin ≤16 goldens to make it pass.

## WHAT IS ALREADY TRUE (don't redo it)
- Position holders wrap at their OWN length (no global 16-modulus). So walk/addressing should need little
  change; the work is the FIXED-16 LITERALS and any length clamps.
- Length values are already LOR-length parameters (1..16 today). The change is the upper bound.

## THE AUDIT — classify every fixed-16 site, do NOT blanket-replace
Grep candidates (trunk shows ~249 hits): `& 0x0F`, `% 16`, `< 16`, `<= 15`, `[16]`, `16 - 1`, `16` used as a
length/clamp. Classify EACH into exactly one of:
  (A) LENGTH/WRAP logic — a pattern/LOR length or phrase wrap. CHANGE to the pattern-length cap (32) or to the
      holder's own length. These are the real work.
  (B) STORAGE SIZE — a per-step array sized to 16 (e.g. [16] step/voice tables, uint16 bit-masks). Widening
      is STAGE 2, NOT this stage. In Stage 1 leave the storage at 16 but make sure nothing indexes past 15
      when length>16 is requested — clamp the length to the storage bound until Stage 2 lands, OR gate
      length>16 behind the Stage-2 storage. Document which.
  (C) DISPLAY constant — the Sands/ring grid cell count, lamp count (16 lamps stays — settled). Leave alone in
      Stage 1 (Stage 3). Do not change the light ring.
  (D) DEPRECATED / unused — src/deprecated/*. Leave untouched.
  (E) UNRELATED 16 — MIDI, counts that happen to be 16 (16 voices, 16 bit fields of something else). Leave.
Record the classification table in the PR description (file:line → A/B/C/D/E + one-line reason). The table is
the deliverable as much as the code: it is what makes the audit reviewable.

## PASS-THROUGH / WRAP RULE (the critical law)
For LENGTH=L ≤ 16 the behaviour must be IDENTICAL to today. For 16 < L ≤ 32 (once allowed) the wrap is at L.
Holder wrap already uses the holder's own length — confirm the (A) sites that compute `% 16` where they
should compute `% L` (the active pattern length), and that the value is the same for L=16.

## LENGTH CAP — gate the new range
- Introduce ONE constant for the max pattern length (kMaxPatternSteps = 32) and derive the length control's
  range from it. Do NOT scatter 32.
- Until Stage 2 widens storage, the usable length is capped at the storage bound (16). Make the cap an explicit,
  single-point switch so Stage 2 flips it to 32 in one place. Document it.

## DELIVERABLES
1. The (A)/(B)/(C)/(D)/(E) classification table (file:line, class, reason).
2. The (A)-class fixes, length-aware, with the single kMaxPatternSteps constant.
3. The explicit storage-bound clamp with a comment pointing to Stage 2.
4. A test: length 1..16 walks wrap at the length (sanity); length >storage-bound is clamped and reports so.

## GATE (must all pass)
- Full battery both SDK legs (rack-sdk-real + UNIFORMITY_STRICT=1; rack-sdk-lin) green.
- ALL ≤16 goldens bit-identical — NO re-pins. (If you must re-pin anything, STOP; that is a regression.)
- warn-sweep clean; anchor audit green.
- Commit per logical change (constant + cap, then each (A)-class fix group), `.c_msg.tmp` + `git commit -F`.

## OUT OF SCOPE (Stage 2+)
Storage widening (uint16 maskPainted_ → 32-bit), the 32-entry per-step arrays, Sands 32-column display, ring
bar-indicator, the 16/32 control UI, persistence of pattern length. Do not start these.
