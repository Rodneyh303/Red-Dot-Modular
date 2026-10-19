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

## THE ROOT OF THE WARP BUGS SPECIFICALLY — warp did NOT copy spread's ARCHITECTURE (code-verified)
Rodney asked: warp is semantically identical to spread (which works) — why couldn't warp just copy
spread? ANSWER (verified): because warp didn't copy spread's ARCHITECTURE, only its surface.
- SPREAD routes EVERYTHING through `SpreadResolver` (src/dsp/SpreadResolver.hpp) — "the SINGLE
  definition of effective spread amount". ONE tested `effective(Inputs)` encodes the FULL delegation
  algebra in ONE place: `eff = base + ownCv·2 + eastCv·2 + lerp(pre,post,tap_v)·send` (non-delegated),
  `eff = clamp(macroBase + tappedDelta)` (delegated). The caller just POPULATES Inputs (base, ownCv,
  eastCv, macroBase, macroSend, macroTap, delegated) and calls effective(). Every subtlety — the
  delegated branch, Macro send, per-voice tap, pre/post lerp, CV mixing — lives inside that ONE
  contract-tested function. THAT is why spread has no Macro→East leak / no V1 asymmetry.
- WARP has NO WarpResolver. It is a bare 15-line `warpedValue` (WarpF.hpp:354) + a crude 2-branch
  delegation (`if delegated: globalWarpF[lane] else warpF[i]`) + a `warpDelegated_[v][l]` flag array,
  with the East/Macro/send/tap resolution SCATTERED across UI + manager + engine. VERIFIED: warp's
  code has 0 send/tap/CV terms; SpreadResolver has 23. Warp REIMPLEMENTED a SUBSET of spread's
  delegation algebra, by hand, incompletely. (Its own comment even says "the SpreadResolver::Inputs
  shape" — it KNEW the model and didn't use it.)
- THEREFORE every warp bug (Macro→East leak, V1 asymmetry, per-voice inconsistency) = warp's bespoke
  resolution diverging from spread's unified one. Spread works because the algebra is in ONE tested
  place; warp breaks because its hand-copy is partial.

### THE WARP-SPECIFIC CURE (do this; it is cleaner + smaller than a path-unification refactor)
Build `WarpResolver` modeled EXACTLY on SpreadResolver: same Inputs shape (base=the lane's warp
curve+w, ownCv, eastCv, macroBase=globalWarpF/W, macroSend, macroTap, delegated), ONE `effective()`
authority, ONE contract test (the SpreadResolver test is the template). Route BOTH executeStep(V1)
and executePolyVoice(V2+) through it, populating Inputs identically. Then warp IS spread, structurally
— the leak/asymmetry can't exist because the algebra is in one tested place, same as spread.
This may make the Stage-2 execution-path unification unnecessary FOR WARP (both paths just call the
resolver) — though the mono/poly execute split is still worth closing for other features.

────────────────────────────────────────────────────────────────────────────────
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

### SEAM 1 — VOICE: it is an EXECUTION-PATH (class-structure) seam, NOT an index seam  [CORRECTED by code-read]
VERIFIED (don't trust the docs' "index residue" framing — checked the code):
- The INDEX base is already CONSISTENT voiceSlot here: `voices[16]` is 0=V1 (SequencerEngine.hpp:134
  "Phase A: voices[0] = V1"), and executePolyVoice's `voiceIdx` is voiceSlot. So this is NOT a
  voiceSlot-vs-polyBank index bug in the execution path.
- THE REAL SEAM is TWO SEPARATE EXECUTION FUNCTIONS — the mono/poly class split the
  convention_unification doc called "the THIRD level (CLASS structure), UN-STARTED":
    · V1 runs through `executeStep` (SequencerEngine.cpp:474), which reads warp HARDCODED as
      `pe.warpRow(0, PL_MELODY)` / `warpRow(0, PL_OCTAVE)` (:553-554). V1-ONLY path.
    · V2..V16 run through `executePolyVoice` (:1056), reading `polyRandomSrc(voiceIdx, ...)` (11
      sites, voiceSlot). The caller loop is `for i=1..numPolyVoices` (:1334) — SKIPS slot 0 (V1).
  So any per-voice feature must be wired into BOTH functions, identically, or V1 diverges. THIS is
  "Macro no-effect on V1 / works V2+": warp is wired one way in executeStep, another in
  executePolyVoice. An INDEX fix would NOT have caught it (both use the right base); the seam is
  that the two PATHS exist at all.
- LIVE LYING COMMENT (delete regardless): SequencerEngine.hpp:132 "voices[0] = voice 2" sits 2 lines
  above :134 "voices[0] = V1" — contradictory, the exact stale-comment smell of the polyBank lie.
- THE MACRO→EAST LEAK on V2+ (incl non-delegated lanes) is a DIFFERENT site — the UI/manager WRITE
  path (Macro setGlobalWarpF / East currentSlot / warpDelegated_ population), which the Stage-1 audit
  must map separately. (05's test passed because it wrote WarpBank::index directly, never this path.)
- ENFORCEMENT GAP: voiceSlot is a bare `int`; nothing stops a wrong base OR a path that forgets V1.
  (Stage 4 strong types + Stage 2 path-unification both bear on this.)
- REMEDY shifts: Seam 1 is closed by UNIFYING the execution path (one per-voice execute that V1 and
  V2..16 both flow through, no hardcoded warpRow(0) vs voiceIdx split) — NOT by renaming indices.
  This is bigger than an index sweep; it is the class-structure merge the docs flagged as un-started.

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

### STAGE 2 — VOICE seam close (Seam 1): UNIFY THE EXECUTION PATH (not an index sweep)
CORRECTED scope (code-verified): the seam is executeStep(V1) vs executePolyVoice(V2+), two paths with
hardcoded warpRow(0) vs voiceIdx. Close it by making V1 and V2..16 flow through ONE per-voice execute
(V1 = slot 0 of the same loop), so no feature can be wired into one path and not the other. Delete the
V1-special executeStep branch where it duplicates executePolyVoice; delete the lying :132 comment.
Convert any genuine DEBT index (0-based at a non-I/O site) to voiceSlot as part of it. Also map+fix the
UI/manager WRITE path (Macro→East leak) found in Stage 1. Gate: uniformity harness green for ALL voice
cells incl V1/V2, driving the REAL execute path (not a direct-index-write bypass).

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
