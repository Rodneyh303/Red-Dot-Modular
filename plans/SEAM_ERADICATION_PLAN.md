# SEAM ERADICATION PLAN — finish the lane/voice uniformity so new features wire in clean

Rodney's call (2026-10, during the warp debug): wiring warp in — SEMANTICALLY IDENTICAL to spread,
which works — detonated days of lane/voice inconsistency bugs (Macro↔East leaks, V1-vs-V2+ asymmetry,
VAR/LEG not following direction, Macro-controls-East then the reverse). That is NOT a warp problem.
It is warp WALKING THROUGH class-promotion SEAMS that were never fully closed. Every future per-voice
/per-lane feature hits the same mines until the seams are gone. So: CLOSE THE SEAMS FIRST, then warp
(and everything after) wires in mechanically, like spread.

This plan is staged markup (Rodney: "has to be a planned markup not one big prompt"). Each stage is
its own CC task with a contract test gate. DO NOT do it as one prompt.

## WHAT A "SEAM" IS (the one defect, four faces)
A distinction that was once real but is now (mostly) obsolete, which the code STILL BRANCHES ON in
some places and not others. The off-by-ones live exactly at the boundary between the two conventions.
Enforced by COMMENT/DISCIPLINE, never by the TYPE SYSTEM or a test — so it can't be reasoned about
(polyBankIndex LITERALLY LIED: name said 0=V2, code returned v-1) and can't be caught before Rack.

## STATE OF THE WAR (what past-you ALREADY WON — verified in current code)
Huge progress already landed — do NOT redo these:
- `polyBankIndex` the lying function: DELETED. ✓ (plans/V16_FLICKER_root_smell.md was the diagnosis.)
- The 4 lane-ordering BRIDGE TABLES (ENGINE_LANE_TO_EDITOR, EDITOR_TO_ENGINE_LANE, SPREAD_TO_BUFFER,
  SPREAD_LANE_TO_EDITOR): DELETED as identity (LaneMapping.hpp Phase 3b comments). ✓
- `MonoSandsParameterManager` (the vestigial mono class): RETIRED. ✓
- Storage collapsed to [16][16]; SpreadInterp speaks unified PL_* order (LANDED 2026-10-16). ✓
- ParameterManager classes down to 2 (MonsoonParameterManager, SandsParameterManager). ✓
Authoritative history: plans/convention_unification_INCOMPLETE.md, plans/V16_FLICKER_root_smell.md,
docs/design/SANDS_LANE_INDEX_AUDIT.md, docs/design/SANDS_CONSOLIDATION.md, plans/sands_consolidation_build.md.

## THE REMAINING SEAMS (verified present in current code — these are what warp trips on)

### SEAM 1 — VOICE: voiceSlot vs raw voiceIdx / 0-based loops (PARTIAL residue)
- VoiceResolver IS the authority (V1=slot0=mono, V2..16=slot V-1; 0-based polyBank DEAD).
- BUT `voiceIdx` still appears 48× in SequencerEngine.cpp and raw 0-based loops (`dv=0..16`) remain.
  SOME is legitimate (the poly execution path genuinely iterates voices) — the AUDIT must separate
  "this is a real per-voice loop using the right base" from "this crosses conventions / is debt".
- THIS IS THE ACTIVE WARP BUG: warp write (Macro/East) ↔ warp read (engine) ↔ warpDelegated_ do NOT
  all speak voiceSlot. Symptoms: Macro no-effect on V1 / works V2+; Macro→East leak on V2+.
- ENFORCEMENT GAP: voiceSlot is an `int`, so nothing stops a raw int / wrong base. (See Stage 4.)

### SEAM 2 — LANE: VAR/LEG (editor 5,6) still SECOND-CLASS (14 special-case sites)
- sands_consolidation_build.md §2 SPEC: lane uniformity = EVERY lane has the COMPLETE feature set
  (direction, direction-MOD, delegation + delegation-mod-input, spread, prob-outs). This was the plan.
- IT WAS NOT FINISHED. VAR/LEG got spread (Consolidation Step 3) but NOT the rest. 14 `VAR/LEG` /
  `l < POLY_LANES` special-case sites remain in MonsoonSandsManager.cpp.
- THIS IS THE ACTIVE DIRECTION BUG: East follows Macro DIRECTION on lanes 0-4 but not 5-6 (VAR/LEG)
  — direction-delegation never extended to them.
- ALL OF IT IS DEBT (Rodney, explicit): there is NO real reason VAR/LEG are second-class for CV,
  direction, or delegation. The "East CV only on 4 lanes" exclusion I earlier called intentional is
  NOT — it's unfinished consolidation. Every VAR/LEG special-case is a seam to close.

### SEAM 3 — the VALUE-store audit (flagged "REMAINING, NOT fixed" in convention_unification_INCOMPLETE)
- The spread-VALUE fetch columns (engine spread store, Monsoon editor spread, global spread) may still
  be order-mismatched vs the unified EDITOR indexing; setMacroSpread's numVoices loop over the 16-wide
  slot bank. Warp ADDED a parallel value store (warpF/warpW/globalWarpF) — it must NOT repeat this.

### SEAM 4 — ENFORCEMENT: distinctions are convention-checked, not type/test-checked (THE ROOT)
- Why seams keep coming back: `voiceSlot`/`laneId` are bare `int`s; the convention is in comments.
  A wrong base compiles. A dropped lane compiles. Caught only in Rack, after the fact.
- There is a `test_voice_conventions.cpp` but it clearly does NOT assert per-FEATURE per-CELL
  uniformity (or warp/direction bugs would have gone red). The test harness is the real gap.

## THE STAGED PLAN (each stage = its own CC prompt, its own contract-test gate, Rack-check between)

### STAGE 0 — BUILD THE SAFETY NET FIRST (no behaviour change)  [do before any refactor]
The lesson from the whole war (convention_unification_INCOMPLETE "Process lesson"): every fix was an
UNVERIFIED guess because no test caught "voice N / lane L behaves like the others". Fix that FIRST.
- Build a UNIFORMITY HARNESS: `test_lane_voice_uniformity.cpp`. For EACH feature that is per-voice-
  per-lane (spread, warp, direction, delegation, CV, prob-out), loop ALL 16 voices × ALL 7 lanes and
  assert the feature resolves/routes IDENTICALLY in shape — no voice special, no lane special.
  Concretely per feature: set a distinct value on (v,l); assert the consumer for (v,l) reads IT and
  (v',l') for all others are UNCHANGED (the no-leak / no-drop assertion). Include v=0(V1) AND v=1(V2),
  and l=5,6 (VAR/LEG) EXPLICITLY — the cells current bugs distinguish.
- This harness is RED on today's code (it catches the warp + direction bugs). That's the point: it
  turns "days of Rack whack-a-mole" into "run the harness, see which cells are wrong." It is the gate
  for every later stage.

### STAGE 1 — AUDIT (inventory only, change NOTHING)
CC produces `plans/SEAM_AUDIT_RESULT.md`: every lane/voice-special site in the Sands/engine/expander
path — each `voiceIdx` / `dv=0` / `pv=0` / `+1`/`-1`-near-voice / `l < 4` / `l < 5` / `l < POLY_LANES`
/ `VAR/LEG` branch / `if (voice==0)` / `kMonoSlot` special-case — classified: (a) REAL per-voice/
per-lane loop on the correct base (keep), (b) DEBT (crosses conventions / drops a lane / wrong base).
NO edits. This is the map; later stages fix against the WHOLE map, not symptom-by-symptom (the thing
that made bugs MOVE instead of die — 6+ failed attempts, documented).

### STAGE 2 — VOICE seam close (Seam 1)
Against the audit: convert every DEBT voice index to voiceSlot (VoiceResolver). 0-based ONLY at the
genuine cable-channel I/O boundary. Fixes the warp Macro↔East/V1 bug as a SIDE EFFECT. Gate: the
uniformity harness goes green for all voice cells incl V1/V2.

### STAGE 3 — LANE seam close (Seam 2): finish VAR/LEG to FULL uniformity
Against the audit + sands_consolidation_build.md §2: give VAR/LEG (and confirm ALL 7 lanes have)
the complete feature set — direction, direction-mod, delegation + delegation-mod-input, spread,
prob-outs. DELETE every `VAR/LEG` / `l < 4|5` special-case that is debt. Fixes the direction bug.
Gate: uniformity harness green for all lane cells incl VAR/LEG.

### STAGE 4 — ENFORCEMENT: make the seam UN-WRITABLE (Seam 4 — the thing that ENDS recurrence)
Introduce STRONG TYPES so a wrong base / dropped lane does NOT COMPILE:
- `struct VoiceSlot { int v; }` mintable ONLY via VoiceResolver (not from a raw int). Consumers take
  VoiceSlot, not int. `warpRow(rawInt)` stops compiling.
- `struct LaneId { int l; }` covering all 7 lanes, one canonical descriptor (merge LaneMapping(dsp)
  + any ui lane-name system if still split). A lane loop that stops at 4/5 can't silently drop 6.
- This converts the WHOLE bug class from "caught in Rack" to "caught by the compiler" — the same leap
  anchor/bind did for panels and contract tests did for the convention war.
Gate: it compiles, uniformity harness green, no raw-int voice/lane index remains in the hot path.

### STAGE 5 — VALUE-store audit (Seam 3) + retire any remaining vestigial bridge
Finish convention_unification_INCOMPLETE's "REMAINING Phase-2 value-store audit"; confirm warp's
value store (warpF/W/globalWarpF) is indexed uniformly and did not re-introduce a mismatch.

### THEN — WARP (and anything after) wires in like spread. The mines are gone.

## PROCESS RULES (from the war's own lessons — non-negotiable)
- Each stage its own commit + Rack check (Rodney has Rack; verification is the bottleneck, not typing).
- The UNIFORMITY HARNESS is the gate for every stage — a green that bypasses the real write/read path
  (e.g. 05's test wrote WarpBank::index directly, never the UI/manager remap → green while Rack broke)
  does NOT count. The harness MUST drive the actual write path and the actual consumer.
- Fix against the WHOLE AUDIT MAP, never symptom-by-symptom (symptom fixes MOVE bugs; proven 6+ times).
- VERIFY against code, never a plausible narrative (this session alone: 3 AI misdiagnoses —
  polyRandomSrc-as-raw, poly-site-as-bug, loop-bounds — each from asserting before reading).
