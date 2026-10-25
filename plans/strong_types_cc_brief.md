# STRONG TYPES (VoiceSlot/LaneId) — CC execution brief

Implements `docs/design/CC_PROMPT_STRONG_TYPES_START.md` (SEAM_ERADICATION_PLAN Stage 2, moved
early). Make a wrong per-voice/lane index a COMPILE ERROR, not a Rack-later bug. The mono ROLE
(V1-as-anchor) is PRESERVED — typed, not removed.

- BRANCH: cut `feat/strong-voice-lane-types` off `lane-expander-refactor`
  (`git checkout lane-expander-refactor && git pull --ff-only && git checkout -b feat/strong-voice-lane-types`).
  PR target: `lane-expander-refactor`. NEVER rebase after commits land.
- SCOPE (Rodney, decided): FULL spec DONE in one PR — core + engine bank accessors + ALL
  module-layer `voices[]` readers. The tighten commit deletes every int path.
- C++20 confirmed green (CI). House style commits: `feat:` / `refactor:`.

## The types (scaffold — mint-only-via-VoiceResolver)

New self-contained header `src/dsp/VoiceLaneTypes.hpp` (NO engine includes — it must be
includable from PatternEngine.hpp and LaneMapping-level code):

```cpp
struct VoiceSlot {
    int v;
    explicit constexpr VoiceSlot(int raw) = delete-ish; // PRIVATE ctor; friend struct
                                                        // dotModular::VoiceResolver mints.
    // == / != only; no implicit int<->VoiceSlot either way (that's the whole point).
};
struct LaneId {
    int l;                          // 0..6, PL_* order (MEL0 OCT1 QMIX2 REST3 ACC4 VAR5 LEG6)
    explicit constexpr LaneId(int lane);
    // == / != only.
};
```

- `VoiceResolver` (src/dsp/VoiceResolver.hpp) gains the mint API:
  - `mono()` → `VoiceSlot{0}` (V1 — the anchor; grep-able, not an anonymous 0)
  - `slot(V)` → `VoiceSlot{V-1}` (V = VOICE NUMBER 1..16, the UI/tab convention)
  - `allSlots()` → iterable range covering slots 0..15 exactly once (poly loops iterate it,
    or loop voice numbers V and mint `slot(V)`)
  - EXISTING int helpers (`voiceSlot`, `polyVoiceSlot`, `slotToVoice`, `kMonoSlot`,
    `kVoiceSlots`) STAY — they keep compiling during migration (spec: "add typed overloads
    first, don't break callers").
- `LaneId` is NOT mint-restricted (spec's own call-site spelling is `LaneId{PL_MELODY}`) but
  construction is EXPLICIT-only. Add an all-7-lanes range (`allLanes()`) so a lane loop
  CANNOT stop at 4 and silently drop VAR/LEG. `EditorLane`/`EngineLane` (LaneMapping.hpp)
  remain as-is for UI callers — unifying them with LaneId is a NOTED FOLLOW-UP (spec's
  "NOT THIS PROMPT").
- static_assert self-checks in the header (the `voiceResolverIndexSelfTest_` pattern):
  `mono().v==0`, `slot(2).v==1`, `slot(16).v==15`, allSlots covers 0..15 exactly once.

## VoiceBank (the voices[] enforcement)

`PolyVoice voices[16]` (SequencerEngine.hpp:140) → wrapper struct with
`operator[](VoiceSlot)` (+ const overload) and, DURING MIGRATION ONLY, `operator[](int)`.
Identical layout (wraps `PolyVoice[16]`). The tighten commit deletes `operator[](int)` —
after it, `voices[0]` does not compile; `voices[VoiceResolver::mono()]` is the spelling.

## Typed overloads (scaffold commit; int versions coexist until tighten)

- `PatternEngine::warpRow(VoiceSlot, LaneId)` / `polyRandom(VoiceSlot, LaneId)` — alongside
  the int versions (PatternEngine.hpp:181 / :146).
- `displayedVoice_` (PatternEngine.hpp:571, plain int) → `VoiceSlot` member + minting
  setter. Set today from StraitsEastSandsVisual.cpp:1167
  (`monsoon->engine.pe.displayedVoice_ = selectedVoice`) — mint there.
- Engine per-voice bank accessors (commit 4, full scope):
  `polyLOR`, `polyLaneTick`, `polyLenE`/`polyOffE`/`polyRotE`, `refreshPolyRandomCell`,
  `polyLaneProbability`, `polyLaneStep`, `effectiveProbability`, plus the
  SequencerEngine.hpp :526 `warpRow(0, s)` (finalRandomByStrand) and :678/:687
  (polyLaneProbabilityAtStep-ish) call sites.
- `SpreadInterp::applyMono/applyPoly` signatures are its own seam — NOTE as follow-up, do
  not convert in this PR.

## The commits (each: suite green + STRICT harness green BEFORE committing)

1. `feat: VoiceSlot/LaneId strong types + VoiceResolver minting + typed overloads (int kept)`
   — types header, mint API, allLanes/allSlots ranges, typed warpRow/polyRandom overloads,
   VoiceBank (both operators), static_asserts, and the minting round-trip assertions in
   test_lane_voice_uniformity.cpp (mono()→{0}, slot(V)→{V-1}, iterator covers 0..15 exactly
   once — the spec's required added assertion). Zero call-site changes.
2. `refactor: PatternEngine surface to VoiceSlot/LaneId` — displayedVoice_ → VoiceSlot +
   setter (StraitsEastSandsVisual call site), internal warpRow/polyRandom sites
   (PatternEngine.cpp :368/:812-818/:908/:917-918 + the promote loops' polyRandom calls),
   recomputeEffective*ForVoice signatures.
3. `refactor: SequencerEngine mono path` — executeStep core (:552-768), legatoCheckpointOnFall
   (:817-819), ghost roll (:999-1000): `voices[0]` → `voices[VoiceResolver::mono()]`,
   `pe.warpRow(0, PL_X)` → `pe.warpRow(VoiceResolver::mono(), LaneId{PL_X})`.
4. `refactor: SequencerEngine poly paths + per-voice banks` — executePolyVoices loop
   (:1348) + executePolyVoice(voiceIdx → VoiceSlot), mode A/B/C/D blocks, the modeB ghost
   block (:865-869, :965-968), nvIdxForVoice/getVariationStepForVoice/getLegatoStepForVoice
   (bank params), and the bank accessor family from above.
5. `refactor: controller + module layer` — MonsoonModeController (SOLE writer of voices[0]:
   :125-131, :302; updatePolyVoiceRest_ loop :47-58 — `i` is a voiceSlot, loop voice numbers
   and mint `slot(V)`), Monsoon.cpp (:950-952, :987-988, :1048-1051, :1181-1187, :1358-1360),
   MonsoonOutputGenerator (:66-75, :211-225), MicroTuning (:184-185), Lantern (:302),
   Intertropical (:171 — the `(v==0) ? eng.gs : eng.voices[v]` ternary). DELETE the lying
   comment SequencerEngine.hpp:136 ("voices[0] = voice 2" above the :138 correction — the
   seam plan flags it "delete regardless"). Comment pass: rewrite `voices[0].*`-in-prose to
   "mono()'s slot" so the tighten's substring pins stay comment-safe.
6. `refactor: test call sites` — ~14 files calling warpRow/polyRandom/voices[0] with ints
   (uniformity, bitcompare, eradication, subgate, mode_b_gate, mode_b_gap, gate_smoke,
   gate_mode_agnostic, per_voice_articulation, probmod_roundtrip, voice_resolver,
   voice_conventions, warp_mirror, warp_all_lanes, warp_voice_isolation, ui_to_resolver,
   macro_own_output). Mechanical minting; tests simulating the controller write
   `eng.voices[VoiceResolver::mono()]`. test_poly_voices has its OWN local PolyEngine —
   untouched.
7. `refactor: tighten — mint-only enforcement` — delete `warpRow(int,int)`,
   `polyRandom(int,int)`, `VoiceBank::operator[](int)`. Grep-confirm DONE: 0 raw
   `warpRow(0,`, 0 bare `voices[0].` (all via mono()), 0 bare-int per-voice index in the
   hot path. Add A5-style structural source-grep pins (the test_warp_all_lanes.cpp:271
   pattern) so the DONE greps are a permanent test: forbidden substrings chosen
   comment-safe, e.g. `pe.warpRow(0`, `warpRow(0, PL_`, `voices[0].` in src.

## Behaviour-identity argument (state in the PR)

Types are compile-time. Every conversion replaces a literal 0 / loop int with the SAME
numeric index minted (`mono().v==0`, `slot(V).v==V-1` — pinned by static_asserts + the
round-trip assertion). The uniformity harness (16 voiceSlots × 7 lanes via the REAL write
path, no-drop/no-leak) is green after EVERY commit — a conversion that changed behaviour
would go red. CPU unchanged (compile-time only; VoiceBank::operator[] inlines to the array
access).

## Gates (all must pass; suite green after each commit)

- `PYTHONUTF8=1 RACK_DIR=../Rack-SDK bash test/run_all.sh` → ALL green (anchor audit included)
- `UNIFORMITY_STRICT=1 bash test/run_all.sh lane_voice_uniformity` → manifest EMPTY, exit 0
- BOTH builds warning-clean: `make RACK_DIR=../Rack-SDK -j16` AND
  `make clean && make RACK_DIR=../Rack-SDK -j16 EXTRA_FLAGS=-DMONSOON_PROFILE`
- bash via `"C:\msys64\usr\bin\bash.exe" -c "export PATH=/mingw64/bin:/usr/bin:$PATH && cd /c/Users/rodney/Documents/Rackbuild/Red-Dot-Modular && ..."`
  — NEVER `|` pipes inside the bash -c string.
- Report: what landed by symbol; the mint API; conversion counts per file; harness + suite
  outputs; commit hashes.
- PR note (per spec): "strong types — wrong per-voice/lane index now un-compilable; mono
  ROLE preserved as VoiceResolver::mono(), typed not hardcoded." Add: CPU unchanged
  (compile-time only), and the noted follow-ups (EditorLane/EngineLane unification;
  SpreadInterp signatures; rate manager builds ON these types next).

## NOT this PR

- EditorLane/EngineLane unification into LaneId (noted follow-up).
- The rate manager (next prompt — builds ON these types).
- The mono ROLE itself (V1-as-anchor is correct — TYPE it, don't remove it).
- SpreadInterp::applyMono/applyPoly signature conversion.
