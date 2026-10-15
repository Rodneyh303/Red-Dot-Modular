# CC BRIEF — "mono is not special" 100% eradication (3 levels) + VERIFICATION FIRST
Branch collapse-16-voices. READ FIRST: plans/convention_unification_INCOMPLETE.md (root cause, 3-level map,
class audit), plans/two_bugs_reframed_v1_is_reference.md, plans/collapse_count_offbyone_audit.md.

## WHY THIS BRIEF IS DIFFERENT (read this — 6+ prior attempts failed the same way)
Every prior attempt "thought it was fixed" but wasn't, because the bugs are BEHAVIOURAL (display/voice) with
NO automated check — so each change was an UNVERIFIED GUESS. DO NOT start eradicating until the verification
harness exists. The harness is PHASE 0 and is MANDATORY. If you cannot make a change verifiable, do not make
it.

## PHASE 0 (MANDATORY, FIRST, its own commit) — build the verification harness
Add a test that makes the voice bugs CATCHABLE without Rack:
1. **All-voices-have-data test:** for N in {2, 6, 15}, set N active voices, roll the dice (fixed seed), and
   assert EVERY voice 0..N-1 (voiceSlot, 0=V1) has NONZERO Sands lane-probability data for each lane
   (rhythm/melody/qmix). This catches the last-voice-zero (Bug A) and any voice reading an unfilled slot.
2. **Voice-consistency test:** for a given selected voice, assert the voiceIndex the STEP-boundary engine
   path computes equals the voiceIndex the VIDEO-rate display path computes (the "two patterns = two voices"
   check). They MUST be equal. This catches the convention off-by-one that causes the jitter.
3. **Bit-compare harness over ALL voices:** a golden snapshot (fixed seed, fixed params) of every voice's
   per-lane output, so ANY eradication step that changes an UNAFFECTED voice's output is caught. (Voices
   that were CORRECT must stay bit-identical; only buggy voices may change, to the CORRECT value.)
Run these in the existing test suite. Commit PHASE 0 alone, green, before any eradication.
Rule for the rest: after EVERY eradication commit, these tests must stay green (or change only in the
expected direction for a bug being fixed). A red test = STOP, that change is wrong. No "I think it's fixed" —
the harness decides.

## PHASE 1 — finish STORAGE (nearly done): the last 8 [15] in src -> [16][16], V1=0. Verify green. Commit.

## PHASE 2 — INDEX/CONVENTION: ONE base = voiceSlot (0=V1) everywhere internal
~238 voiceIdx + 20 polyIdx + 52 nvIdx usages across 11 files; ~21 +1/-1 conversions; ~4 zero-based pv/dv
loops. Convert ALL internal voice indices to voiceSlot (0=V1, 1..15=V2..V16). 0-based conversion ONLY at the
genuine cable-channel I/O boundary (the ~20 legit voiceIdx+1 for poly-cable ch0=mono — KEEP those, mark
them). Eliminate voiceIdx/polyIdx as INTERNAL names. Do it in SMALL commits (one file or one cluster at a
time), harness green after each. The harness's voice-consistency test will catch any site left on the old
base.

## PHASE 3 — CLASS structure (the THIRD level — this is likely where V1-special LIVES)
- RETIRE MonoSandsParameterManager (only 3 refs, 1 in deprecated/ — nearly dead). Fold any live use into the
  general per-voice parameter path. V1 must use the SAME parameter path as poly.
- MERGE MonsoonSandsManager + the redundant ParameterManager(s) so there's ONE per-voice param manager, not
  mono+poly variants.
- MERGE LaneMapping (src/dsp) + SandsLaneNames (src/ui) -> ONE canonical lane descriptor (32 refs).
- DELETE the 14 lane identity-conversions + EL2ENG (no-ops converting between now-identical reps).
- LEAVE functional managers: UIManager, ScaleManager, PersistenceManager, LockManager.
Each retirement/merge = its own commit, harness green after.

## PHASE 4 — the two bugs (likely ALREADY FIXED by Phases 2-3; verify, else targeted)
- Bug A (last voice zero): if the harness all-voices test is green, FIXED.
- Bug B (poly jitter / V1-only-smooth / flips-between-two-patterns): if the voice-consistency test is green
  AND V1 no longer has a special path (Phase 3), it should be fixed. If jitter PERSISTS with voice-
  consistency green -> it's the TIMING race (recompute vs refreshPolyRandomCell): reconcile so each poly
  random_[v] has ONE writer (recompute feeds refreshPolyRandomCell's input; refresh is sole final writer),
  matching V1's single-writer path. Remember: melody/rhythm/qmix jitter is ONE bug (shared path) — fix once,
  all three go smooth TOGETHER (the crisp test).

## HARD RULES
- PHASE 0 harness FIRST, mandatory. No eradication before it's green.
- SMALL commits, harness green after EACH. A red test = that change is wrong, revert/fix before moving on.
- NEVER say "fixed" based on belief — the harness or a stated Rack test decides.
- Bit-identical for voices/states that were already correct; buggy voices change to CORRECT only.
- Leave Category 2 (POLY_*_PARAM enums, panel, gen_straits.py) — intentional, no preset/UI migration.
- 3 platforms green.
