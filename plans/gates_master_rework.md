# Gates master rework — coordinating plan (feat/subgate-poly-width)

**Read this first; it sequences the per-feature plans and reconciles them with the SETTLED docs.**
The design docs were finalised AFTER much of the code was written, so where code and doc conflict,
the DOC wins on intent — report conflicts, do not trust stale code. Suite green now; keep it so;
one commit per phase.

Per-feature plans (detail): `legato_gate_gap_fix.md`, `subgate_dev_plan.md`,
`subgate_ghost_dev_plan.md`, `mode_b_legato_rework.md`, `east_varleg_cv.md`.
Settled specs (authority): `docs/design/LEGATO_GATE_GAP_BUG.md` (FINAL/RESOLVED/DECISION/menu
sections), `docs/design/GATE_SUBDIVISION_STEP_GATE.md` (settled sections), `RHYTHM_BEHAVIOUR_TOGGLES.md`.

## What's ALREADY on the branch (the forgotten push — verify, don't rebuild)
Commits 10f4c68..50580ed implemented: subgate ghost plumbing; Schmitt gate edges; a two-toggle
legato model (generatedRestBeatsLegato / incomingRestBeatsLegato); the 1ms timer then its removal;
and perVoiceArticulation (the old binary per-voice VAR/LEG option). So several phases below are
PARTIALLY built — reconcile against the settled docs, finish the gaps.

## Phase 0 — recon, write nothing, REPORT
Diff existing code vs the settled docs and report disagreements. Specifically:
- Does the coded two-toggle model match the SETTLED MENU (LEGATO_GATE_GAP_BUG.md CONTEXT-MENU LAYOUT):
  **"Generated rest beats legato"** (default TRUE), **"Tie across rests"** (default TRUE; TRUE=BRIDGE,
  i.e. incoming rest does NOT beat legato — verify not inverted), **"Advance playhead on tie-into-rest"**
  (default FALSE)? Report naming/polarity/missing.
- Is the ms timer + slurForward bridge FULLY removed (RESOLVED section)? Behaviour must be: FALSE has
  NO self-bound (rest is the only brake across long gaps — do NOT re-add a cap/timer); strict is
  overlap-only with NO <=1-sample allowance.
- Which subgate rules exist vs missing (GATE_SUBDIVISION settled): two inputs no-normalling; envelope
  classification (in-gate=ratchet, gap=ghost); straddle clip (in-gate cells end at the fall; ghost may
  tie through the rise via its own slurForward roll); boundary-COINCIDENCE table (ghost suppressed on
  both main-gate edges, resumes >=1 sample into gap; ratchet included on rise, suppressed on fall);
  ghost-consumes-gap-slur (a sounding ghost ties the pending slur in, rested ghost transparent);
  correlated ghost PLACEMENT (per-voice variation correlated to mono, +-1); playhead advances on every
  ratchet/ghost onset (edge-driven; the ONE exception is the tie-into-rest toggle).

## Phases to COMPLETE after the report (sequence; adjust to what recon finds)
1. **Legato menu → settled 3-toggle layout.** Finish/rename to the two-section menu; ensure
   generatedRestBeatsLegato rename handles patch-JSON (keep old key or migrate; say which). Add
   "Advance playhead on tie-into-rest" (default FALSE) — the SOLE violation of edge-driven playhead,
   opt-in.
2. **gate=quantiser MODE-AGNOSTIC invariant + test.** One gate path, no "if quantiser mode" special
   cases; q-mix only switches pitch source. Add a test asserting gate/rest/legato/accent output is
   bit-identical for the same gate input across the q-mix range. (GATE_SUBDIVISION INVARIANT.)
3. **Subgate inputs** (ratchet + ghost, no normalling; ghost=gate→sustained, trigger→blip;
   ratchet=trigger-or-gate onset-only) — if not complete.
4. **Subgate boundary rules** (straddle clip + coincidence table + ghost-consumes-gap-slur) — if missing.
5. **Correlated ghost placement** (per-voice variation correlated to mono reference via the copula
   machinery; playhead mono; rest/legato/accent shape survivors per voice).
Reverse-safe throughout; keep the forward/reverse bitwise test green.

## perVoiceArticulation — LEAVE IT, it is consolidation-redundant
`SequencerEngine::perVoiceArticulation` (default OFF) is the OLD binary per-voice VAR/LEG option (the
feared "soup" — uncorrelated per-voice variation, left as an opt-in flag). The Gaussian-copula
CORRELATION MATRIX supersedes it: graded per-voice variation correlated to mono (+1 shared / 0
independent / -1 interlocking) replaces the binary on/off, safely. So it is slated for REMOVAL at the
Sands consolidation (SANDS_CONSOLIDATION.md), NOT now. Do not build on it or extend it; confirm it still
works and flag it redundant in the recon report.
