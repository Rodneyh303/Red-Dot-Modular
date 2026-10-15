# Voice-convention UNIT TESTS (not asserts — tests live in test/, out of prod code)
The root of this session's hardest bugs (V2 then V16 flicker) was a LYING abstraction
(VoiceResolver::polyBankIndex: name/comments say 0=V2, code returns v-1=voiceSlot). static_assert is the
WRONG tool: the invariant is currently FALSE so it wouldn't compile, and runtime asserts don't belong in
shipped DSP. The right tool = behavioural UNIT TESTS that pin index<->voice mappings by ACTUAL behaviour.
These catch lying conventions no matter where they hide, run in the fast harness, and never touch prod.

## Why tests beat asserts here
- OUT of prod code (test/ only) — zero plugin footprint.
- Catch the lie you DON'T know about: a round-trip test fails with "V16 read V15's data" even when every
  local number looks right (which is exactly what fooled everyone — the numbers were locally plausible).
- Run every commit in the parallel+precompiled run_all.sh.
- They ARE the verification layer whose absence made V2/V16 take forever.

## The four tests (add to test/)
### 1. VOICE ROUND-TRIP IDENTITY (the big one — catches the whole class)
For each voice V in V1..V16: write a UNIQUE sentinel into V's data via the writer path, then read it back
via EVERY accessor/display path; assert you get V's sentinel, NOT V±1's. Any convention mismatch anywhere
(lying polyBankIndex, wrong-index caller, two-writer) surfaces as "voice N's data appeared under voice M".
One test, whole class.

### 2. ACCESSOR CONVENTION CONTRACT (executable documentation of the TRUTH)
For each per-voice accessor, pin index->voice explicitly:
  polyRandom(slot=0)=V1, (1)=V2, ..., (15)=V16          [voiceSlot, 16-wide]
  pubSlewedPoly*[0]=V1 ... [15]=V16                      [voiceSlot]
  laneTickV_[0]=V1 ... [15]=V16                          [voiceSlot — confirm]
  getSpread(slot)/editor.spread/macroOwn/laneDir         [voiceSlot]
  polyLenE/OffE/RotE(bank), polyLaneSign(bank)           [CONFIRM: 15-wide 0=V2, or 16-wide voiceSlot?]
  polyBankIndex / polyVoice / currentSlot / voiceSlot    [pin each: what does it RETURN for V2 & V16?]
If an accessor's real convention ever drifts from what callers assume -> test fails. Replaces the lying
comments with executable truth.

### 3. BOUNDARY-VOICE COVERAGE (V1, V2, V16 explicitly)
Every per-voice test runs for the FIRST poly (V2) and LAST (V16) by name — that's where off-by-ones and
lying conventions manifest (we hit both). A test that only checks a middle voice misses exactly these.

### 4. STABILITY (the flicker catcher)
Read a voice's display/bar value twice with a refresh between, no input change -> assert EQUAL. Catches the
flicker class (value alternating frame-to-frame = two-writer or render issue). Run for V16 specifically.

## How this answers "where else are the foundations rotten"
Write test #1 (round-trip) across ALL accessors, run it: EVERY rotten foundation announces itself as a
failing test ("accessor X maps V16 -> slot 14"). You don't HUNT the rot — the test finds it, once, and
guards against regression forever. Foundation-hardening as tests, not vigilance, not prod asserts.

## Sequence (rested day)
1. Write test #2 (accessor contract) FIRST — it forces you to determine + pin each accessor's TRUE
   convention (resolving the polyBankIndex lie in the process). Some will fail -> that's the rot map.
2. Write test #1 (round-trip) -> catches cross-path mismatches.
3. Add #3/#4 coverage. Fix each failure -> conventions now truthful AND guarded.
4. THEN the V16 flicker is either already caught (#1/#4 fail -> fix shown) or isolated to render (#4 passes
   but visual flickers -> render bug, not data).
