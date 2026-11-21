# ARRAY → CONTAINER AUDIT — the honest inventory, and the staged plan

Request (Rodney): "take a look at the codebase especially patternengine, sequencerengine, change alley,
straits visual etc. Pretty much all use stack based arrays instead of heap based vectors. It's a wonder
the module runs at all. Can you do an audit and work out a plan to switch to modern vectors instead of
legacy arrays."

Method: grep inventory over `src/` (403 C-array declaration sites across 69 files), full reads of the
named targets (PatternEngine, SequencerEngine, Change Alley V2 + Transforms, Straits visuals), the
coupling plane (MonsoonSandsManager, SweepSigGate), the constants headers (LaneMapping, VoiceResolver,
TuningTable, CATrajectoryBuffer), and the test battery manifest (`test/run_all.sh`). Every claim below
carries a file:line. Verified against code, not narrative — per CLAUDE.md.

## THE VERDICT, UP FRONT

1. **The premise is wrong in a way that is good news**: these arrays are not stack arrays. The engines
   are direct members of the Module class (`SequencerEngine engine;` Monsoon.hpp:796), and Rack
   heap-allocates every Module instance — the arrays live in the heap object, once per module, for the
   module's lifetime. No stack pressure, no overflow risk from them.
2. **A wholesale switch to `std::vector` for engine state would be a regression**, not a modernization:
   it would put per-array heap allocations on the audio thread, break the byte-coupled snapshot plane
   (see THE CONSTRAINT), and buy nothing — the capacities are design constants (16 voices × 7 lanes ×
   32 steps), not growth data.
3. **The real modernization debt is real but different**: magic capacities (`16`/`7` spelled three ways
   for the same concept), uninitialized function-local scratch, and hidden per-thread shared state
   (`static thread_local` buffers). The modern form that fixes all three **without** the regression is
   `std::array` + the existing named constants — which the codebase already proves works on the hottest
   path (`PhiloxRng.hpp` is 100% `std::array`, audio-thread, zero allocation).
4. The plan is staged P-A…P-E, battery-gated, bit-identical in behavior, and sequenced to NOT collide
   with STEPS_32 (which is rewriting the very literals this audit touches).

## 1. THE PREMISE, CORRECTED — where these arrays actually live

- Rack constructs modules with `createModule<T>()` — one heap allocation holding the whole Module
  object. `SequencerEngine engine;` (Monsoon.hpp:796) and `PatternEngine` (included at Monsoon.hpp:27)
  are direct members, so every `[16]`, `[16][16]`, `[16][7][16]` member is inline storage inside that
  heap block: contiguous, cache-friendly, allocation-free, freed with the module.
- The only true stack arrays are **function locals**, and they are tiny: `int8_t motionV[16]`
  (SequencerEngine.cpp:1282/1341/1368/1522), `float mR[16]…mL[16]` (~448 B, PatternEngine.hpp:403),
  `uint8_t tmp[16]` scratch (ChangeAlleyTransforms.hpp:55/153/198/252/316/343…), `int stepArr[7]`
  (StraitsSandsMacroVisual.cpp:1275/1301), `int rungs[NUM_NOTE_VALUES]`/`float weights[…]`
  (PatternEngine.cpp:354/371). Kilobytes, not megabytes. "It's a wonder the module runs at all" — it
  runs because this layout is actually the CORRECT layout for a Rack DSP engine.
- The third category is neither stack nor object: `static thread_local` function buffers
  (PatternEngine.hpp:409 `pR[16][16]…pL[16][16]` ≈ 7 KB TLS per worker thread; :916 `buf[1024]`,
  :929 `buf[512]`, :939 `buf[256]` ≈ 7 KB more) and one file-static mutable profiler counter
  (StraitsSandsMacroVisual.cpp:50). These are **hidden shared state** — see Finding C.

Footprint of the big residents (computed, per Monsoon instance): the four 7-plane float groups alone —
`slewed*`/`preRemapSlewed*`/`pubSlewedPoly*`/`*Source`, 28 × [16][16] floats (PatternEngine.hpp:368-369,
627-629, 641-643, 669-675) = 28,672 B; `random_` + `warped_` [16][7][16] floats (:108, :180) = 14,336 B;
three `ZWindowCache` (double z0/z1 [16×64], :574-587) = 49,152 B. That is ≈ 90 KB in PatternEngine's
largest planes alone — trivial as heap-resident module state, catastrophic as per-call stack, which is
exactly why the member-not-stack fact matters.

## 2. THE INVENTORY (named scope)

| File | C-array sites | Already modern | Character |
| --- | --- | --- | --- |
| dsp/engines/SequencerEngine.hpp | 42 | — | engine state, fixed capacity, mixed sizing discipline |
| dsp/engines/PatternEngine.hpp | 17 (+5 in .cpp, +4 TLS buffers) | — | engine state + hidden TLS scratch |
| dsp/engines/RhythmDecisionCore.hpp | 6 | — | engine state ([16] graph scratch) |
| dsp/engines/SequencerEngine.cpp | 4 | — | uninitialized function-local scratch |
| dsp/ChangeAlleyTransforms.hpp | 10 | — | transform scratch, hard `16`s (should be `CA::N_VOICES`) |
| MonsoonChangeAlleyV2.hpp | 39 | `std::vector<int> connectedHostIds_` (:62) | engine + triggers + undo ring, **named-constant sized** (`CA::N_VOICES`, `CA::N_ROWS`, `CA::TYPES`) — the best discipline in the audit scope |
| StraitsEastSandsVisual.cpp/.hpp | 12 (4 + 8) | 1 vector (:96) | UI thread; `char rangeLabel_[16]`, local seen-flags |
| StraitsSandsMacroVisual.cpp/.hpp | 8 (5 + 3) | 2 vectors (:98, :106) | UI thread; file-static profiler counters (:50) |
| ui/SweepSigGate.hpp | 7 | — | snapshot struct — byte-mirror of engine arrays |
| dsp/managers/MonsoonSandsManager.{hpp,cpp} | 13 | — | the memcpy coupling plane |

Whole-repo: 403 declaration sites / 69 files; 106 `std::vector`/`std::array` occurrences / 33 files.
This is a mixed codebase, and the modern idiom already exists in it (PhiloxRng, the UI vectors,
`WeightCache`, the CA constants).

## 3. THE FINDINGS (the debt that is actually worth paying)

**A. Sizing-discipline split — the same capacity, three spellings.** `NUM_STRANDS = 7`
(LaneMapping.hpp:50, "(was 6)"), `PL_LANES = 7` (PatternEngine.hpp:135, SequencerEngine.hpp:788, both
`static_assert`ed against `kLaneIdCount`), `kVoiceSlots = 16` (VoiceResolver.hpp:59,
SequencerEngine.hpp:505), `CA::N_VOICES = 16` (CATrajectoryBuffer.hpp:41), `MAXN = 24`
(TuningTable.hpp:24) — and meanwhile: `uint8_t spreadTargetMode[7]` "7 poly lanes"
(PatternEngine.hpp:274), `int macroLOR_[7] = {16,…}` "7 poly lanes" (SequencerEngine.hpp:359) sit
THIRTY LINES from siblings sized `[dotModular::NUM_STRANDS]` (:355-358) and `[kVoiceSlots]` (:506).
`cachedPolySpread[16][7]` (PatternEngine.hpp:115). ChangeAlleyTransforms uses bare `16` for what is
`CA::N_VOICES`. The failure mode already fired once and is on the record: `macroLaneSign_` carries the
comment "NUM_STRANDS-sized (was 6-elem → LEGATO=0 pre-reset)" (SequencerEngine.hpp:356) — an
under-sized magic array silently zeroed the LEGATO lane. Every remaining magic `16`/`7` is that bug
waiting for its next rename.

**B. Uninitialized function-local scratch.** `int8_t motionV[16];` is declared bare and passed by
pointer to `foldPerVoiceAdvance_` / `advancePlayhead` / `recomputeLaneTicks`, then read over a
caller-computed range (`for (i = 1; i <= numPolyVoices && i < 16; ++i) if (motionV[i] != 0)…`,
SequencerEngine.cpp:1282-1288; same shape at :1341, :1368, :1522). Correctness currently rests on the
callee's fill discipline surviving every future edit. `ChangeAlleyTransforms.hpp` has the same shape
seven-plus times (`uint8_t tmp[16];` :55, :153, :198, :252, :316, :343, :373 — filled per-branch,
read after). `std::array<T,N> x{};` deletes this entire bug class at zero cost (zero-init is the
default, no decay, same codegen).

**C. Hidden shared state.** `static thread_local float pR[16][16]…pL[16][16]` (PatternEngine.hpp:409)
and the three `buf` TLS blocks (:916/:929/:939) are per-THREAD, not per-instance: every Monsoon (and
every expander sharing the worker thread) reads/writes the same ~14 KB of TLS. Safe only while the
fully-overwritten-every-call discipline holds; invisible in the object; impossible to test
per-instance; a debugger shows one buffer serving N modules. The file-static
`uint64_t monsoonProfGateWhy[kWhyCount]` (StraitsSandsMacroVisual.cpp:50, incremented :1286) has no
other reference in `src/` — dead or debug-only, and mutable-file-static besides.

**D. Byte-coupled consumers (constraint, not defect).** The UI signature/snapshot plane copies engine
arrays BY `sizeof`: SweepSigGate.hpp:129-144 (`sig.step`, `macroOwnSpread`, `globalWarp*` ×5),
MonsoonSandsManager.cpp:225-226, 917-918, 1058-1065, 1163-1179+ (`s.swRhythm…s.prQmix`, `s.row`,
`img.macro*` — 20+ sites). These `sizeof`-memcpy sites are the "digest" plane the Golden Law protects.
`std::array` keeps every one of them compiling byte-identical (`sizeof(std::array<T,N>)` ==
`sizeof(T[N])`, trivially copyable); `std::vector` silently changes every one of them into a
24-byte handle copy. **This is the single decisive fact of the audit.**

**E. Real-time law.** `process()` runs on Rack's audio worker thread. A `std::vector` member
constructor-allocates once per array (dozens of allocations per module load), can reallocate on first
resize mistakes, adds a pointer indirection on every plane access, and makes the engine state
non-trivially-copyable for the snapshot paths. None of that is survivable *policy* for the engine;
all of it is unnecessary, because no engine array is dynamically sized by design — the 16×7×32 shape
IS the instrument (STEPS_32 exists to change that shape by DECISION, once, on purpose).

## 4. THE PLAN — staged, battery-gated, bit-identical

Exemplar rule for every stage: do what `PhiloxRng.hpp` already does — `std::array`, named constants,
zero allocation, zero behavior change. Verify per CLAUDE.md: `bash test/run_all.sh` green (it covers
every target: test_PatternEngine, test_change_alley_transforms, test_ca_*, test_sweepsig_gate,
test_rhythm_decision_core, test_lane_*, test_warp_*, …) + the CXX20 warning sweep + `needs Rack
feel-test` only for the panel-side items.

**P-A — Constants first (no type changes, no behavior).** In the named files, replace magic `16` →
`kVoiceSlots` / `CA::N_VOICES` and magic `7` → `PL_LANES`/`NUM_STRANDS` (whichever the file already
speaks), magic `24` → `TuningTable::MAXN`. Order: ChangeAlleyTransforms.hpp, PatternEngine.hpp,
SequencerEngine.hpp/.cpp, RhythmDecisionCore.hpp, the Straits visuals. The `static_assert`s
(VoiceResolver.hpp:178, SequencerEngine.hpp:795) hold the door. Risk: near zero. Payoff: the STEPS_32
resize and any future lane change stop being find-and-replace hunts.

**P-B — `std::array` for member state, file by file.** Mechanical: `T name[N]` →
`std::array<T, N> name{};`. Index syntax unchanged; `sizeof` sites unchanged (Finding D survives
untouched); adds `.fill()`, `.size()`, range-for, no decay. Sequence: PatternEngine.hpp → battery →
SequencerEngine.hpp → battery → RhythmDecisionCore.hpp → ChangeAlley state (already
constant-sized; the undo ring `undoRing[UNDO_RING]` becomes `std::array<TransformUndoSnapshot,
UNDO_RING>`) → the two Straits visuals → the mirror structs in SweepSigGate.hpp / MonsoonSandsManager
(their members must become `std::array` IN THE SAME COMMIT as the engine members they memcpy, so the
`sizeof` pairs stay in lockstep). 2-D/3-D: `std::array<std::array<float, 16>, 16>` — or, better, a
local `template <class T, size_t Rows, size_t Cols> using Matrix2 = …` alias so the call sites stay
readable.

**P-C — Scratch discipline.** (1) Every function-local `T x[N];` that crosses a call boundary
(`motionV` class, CA `tmp[16]` class) becomes `std::array<T, N> x{};` — the zero-init is the fix.
(2) The TLS buffers (Finding C) become **instance members** (`std::array`) unless a measured
footprint objection appears — ~14 KB/instance against ~90 KB already resident is not an objection.
If a buffer must stay shared, it stays `thread_local` **with a debug canary** (sentinel-fill in
DEBUG, assert-overwritten) so the overwrite discipline becomes testable instead of assumed.
(3) `monsoonProfGateWhy` gets a reader or a deletion (dead state is a finding, not furniture).

**P-D — Where `vector` is genuinely right (and already used).** UI-thread dynamic collections keep
and continue to use `std::vector`: `connectedHostIds_` (MonsoonChangeAlleyV2.hpp:62 — genuinely
variable: the expander ring), `pendingSpreadArcs` (StraitsEastSandsVisual.hpp:96,
StraitsSandsMacroVisual.hpp:98), `leftAttenuverters` (StraitsSandsMacroVisual.hpp:106). The rule
going forward: **vector off the audio thread, or `reserve()`d in the constructor and size-frozen
before `process()` ever runs; never as engine-state member storage.** The audit found zero
engine-state candidates that need vector — the honest count of "legacy arrays that should become
vectors" is **zero**; the count that should become `std::array` is ~400.

**P-E — Guardrail, so it stays modern.** A small lint test added to `test/run_all.sh` (the repo
already lints this way — `lint_probmod_vocabulary.py`, `audit_anchor_bind.py`): fail the battery on
any NEW bare C-array declaration in `src/dsp/**` and `src/ui/**` outside an explicit allowlist
(NVGcolor tables, `char` string buffers). Mechanical, testable, self-enforcing.

## 5. WHAT THIS PLAN DECLINES (the honest no)

- **Vector conversion of engine state** — declined on Findings D + E: breaks the byte-coupled
  snapshot plane in ~40 places, puts allocations and indirection on the audio thread, and pays for a
  flexibility the instrument does not have (capacities are design constants; when they change — 32
  steps — they change by decision, once, in one place: the constants P-A names).
- **Same-series layout changes** — none. The digest/snapshot plane is byte-layout-sensitive (Golden
  Law); this series is type-and-constant only, bit-identical by battery.
- **A big-bang rewrite** — declined: five small PR-able stages, each green before the next.

## 6. SEQUENCING (the collision warning)

P-A through P-E touch the exact literals STEPS_32 is currently resizing (16 → 32 across these same
arrays). Running both trains on the same files at once is the merge-conflict storm the Sequencing
section of STEPS_32_PLAN.md already forbids. Therefore: **this series starts only after the STEPS_32
resize commit (its P1–P4) has landed**, cut as its own branch off the live branch per CLAUDE.md
branch discipline, one stage per PR, battery green per stage. The audit doc itself is the only thing
landing now.
