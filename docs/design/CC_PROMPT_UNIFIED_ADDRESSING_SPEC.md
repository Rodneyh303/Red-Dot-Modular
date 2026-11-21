# CC PROMPT — unified pattern addressing (global/voice/lane nested maps) + voice LOR — STAGED

Design authority: UNIFIED_ADDRESSING_SYSTEM.md (489 lines — the FULL worked design; LOR defs, nesting,
direction composition, output-mask hierarchy, musical justification). THIS doc is the CC-executable staged
spec. POST-RELEASE, major-but-clean. Branch off lane-expander-refactor. Build Stage 1 (pure refactor, zero
behaviour change) FIRST and prove bit-identical before any new behaviour. Hold the HARD CONSTRAINT
throughout.

## HARD CONSTRAINT (do not violate) — ADDRESSING/READ ONLY
Voice LOR changes only WHERE each voice READS its probability (an addressing transform on the read
position). It does NOT change generation, gate/subgate logic, legato/tie timing, correlation, or anything
downstream. Voices still step on the same clock/grid — they READ DIFFERENT PROBABILITY WINDOWS at the same
step times (content/window staggering, NOT time-displacement). This is what keeps it cheap and avoids the
fiendish per-voice gate-timing interaction.

## LOR DEFINITIONS (authoritative — from the design doc)
- L = LENGTH (number of 1/16 steps the window spans). O = OFFSET (which step the window starts from). R =
  ROTATION (phase into the window at phrase start). O+L = the window; R = phase into it.
- Read-position resolver TODAY: getStrandIdx(counter, L, O, R) (SequencerEngine.cpp ~:42) applies rot/off.
  Lane LOR is per-strand (lorRef / polyLORRef, defaults L=16,O=0,R=0 at :112-150). Global = PATTERN_LENGTH.

## STAGE 1 — refactor global+lane into the NESTED-MAP model, VOICE MAP AT IDENTITY (zero behaviour change)
- Introduce the composition: read_position(voice, lane) = laneLOR( voiceLOR( globalLOR( counter ) ) ).
- RE-BASE lane LOR: today lane R references GLOBAL step 1; change it to reference VOICE step 1 (map VOICE
  step → lane step). With voiceLOR at IDENTITY, global step == voice step, so the re-based lane read is
  EXACTLY today's. 
- voiceLOR = IDENTITY by default (new level, does nothing yet).
- globalLOR = the existing global length/offset, now the top of the chain.
- GOLDEN PIN (mandatory, the whole point of Stage 1): BIT-IDENTICAL reads to today across the full golden
  battery with voiceLOR at identity. Prove the refactor reproduces current behaviour BEFORE any voice LOR
  carries a setting. No new behaviour in Stage 1.

## STAGE 2 — add VOICE LOR (the global→voice map), identity-default
- Per-voice L/O/R + direction, the global→voice map. Macro = GLOBAL voice range (all voices); East =
  PER-VOICE; per-voice DELEGATION between them — REUSE the spread delegation machinery
  (SandsModifierResolver, the Macro/East per-voice delegation, "one axis over"). Identity by default →
  Stage-1 goldens still hold until a voice LOR is set.
- NESTING ANCHORS (from the doc, resolved): voice R = where the voice reads when GLOBAL is at step 1; lane
  R = the lane's position when the VOICE is at step 1. So voice R PROPAGATES to the lanes (shifting voice R
  shifts the voice-step timeline → moves when lanes hit their rotations). (Voice R is no longer inert.)
- DIRECTION COMPOSITION (forced by nesting — pin exactly): effective_dir = playhead_dir × voice_dir ×
  lane_dir (instantaneous SIGN PRODUCT; even reversals → forward, odd → backward). Do NOT flip lane
  direction when the voice reverses — it inherits by composition. Ping-pong/pendulum = instantaneous sign
  per level flipping at its own window boundary; endpoint pre/post-direction rule (each level in its own
  frame) makes the sign definite at turnarounds; pendulum = ping-pong minus the second endpoint read
  (per-level turnaround-style setting). See the doc's direction section for the full rules + worked cases.
- Reversibility: each level is a deterministic LOR transform; forward = compose maps, reverse = compose
  inverses. Validate round-trip BIT-EXACT.

## STAGE 3 — UI: the thin voice-range LED lane on Sands
- An extra THIN lane in the Sands LED grid, CONSTANT block heights (flat lit = in-range step; dim = out of
  range) — distinguishes it from the probability-magnitude lanes. Animates with the playhead (shows the
  voice reading its window in time); shows LOR modulation live. Direction = arrow/orientation. TOP of the
  grid (the frame the probability lanes nest within). NO shrinking the probability lanes, NO dropping
  Sands/helix graphics. Reuse existing lane rendering + animation (constant height).

## STAGE 4 — the OUTPUT-MASK hierarchy (ONE mechanism, not four modes) — after Stages 1-3
From the doc's "containment hierarchy / one mechanism" conclusion. Implement ONE output layer: a per-step
play/mute MASK + a rotation-source flag. The "modes" are configurations, NOT separate code paths:
- TWO mask KINDS: (1) DYNAMICALLY-SIZED (computed from O/L each change — extent tracks the range =
  absolute-1, the range-as-mask); (2) FIXED ROTATABLE (stored painted pattern + rotation offset; rotation
  SOURCE = static [abs2] or follows-R [abs3]). Cycle = all-on degenerate mask (today's behaviour).
- OUTPUT-GATE ONLY: zero the voice's GATE and CV OUT where the mask mutes — NOT the modulation CV INPUTS,
  NOT generation, NOT correlation (same safe output-stage mute as everything else here). Marginals/
  correlation UNTOUCHED.
- Gate-cut at mask/range END respecting PHRASE WRAP (the doc's rule): cut at the range's ACTUAL end, not
  mechanically at step 16; a wrap INSIDE the range carries the gate across normally (with the applicable
  tie-across-rest context-menu options); only cut at 16 if step 1 is outside the range.
- MASK↔ROTATION anchoring (the doc's per-step rule): anchor = mask-step-1 pinned to the BASE (unmodulated)
  rotation, re-pinned only when the base knob changes (per-step change detection, stable between changes);
  modulation = CV rotation offset applied relative to the current anchor. mask pos = latest_base_anchor +
  current_mod_offset.
- Delegation: GLOBAL mask = Macro; PER-VOICE mask = East (same split). Painted via click-toggle on the
  range lane's 16 cells. UI: possibly NO explicit 4-way switch — paint + one follow-R toggle; behaviour
  follows from what's painted (all-on=cycle, contiguous=abs1, arbitrary=abs2/3). Optional named presets.

## FIX WITH THIS SYSTEM (not before) — global length/offset doesn't wrap the Sands read
KNOWN ISSUE: changing GLOBAL length/offset wraps the PLAYHEAD but Sands keeps reading/showing the full 16.
The global-level LOR should BOUND the reads beneath it. Fix as PART OF Stage 1 (the nested maps rebuild the
read-position logic; the correct global wrap falls out). Sub-question to settle then: Sands displays full-16
with the active window highlighted, vs only the active window (ties to STEP_COUNT_DECISION.md /
LIGHT_RING_32_STEPS.md).

## DONE =
- Stage 1: nested-map composition in place, lane LOR re-based to voice-step origin, voice map identity;
  BIT-IDENTICAL golden battery (the regression proof); global length/offset now wraps the Sands read.
- Stage 2: voice LOR (per-voice L/O/R+direction, Macro/East + delegation via spread machinery), nesting
  anchors + sign-product direction + ping-pong/pendulum endpoint rule; round-trip bit-exact.
- Stage 3: thin constant-block animated voice-range LED lane on Sands.
- Stage 4: the one mask mechanism (two kinds + rotation-source flag), output-gate-only, phrase-wrap cut,
  base/mod anchoring, Macro/East delegation.
- HARD CONSTRAINT held at every stage (addressing/read + output-gate only; no generation/gate/correlation
  change). run_all + UNIFORMITY_STRICT green; goldens hold at every identity pin.
