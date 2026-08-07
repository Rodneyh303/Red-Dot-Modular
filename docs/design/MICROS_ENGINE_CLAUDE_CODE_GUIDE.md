# Micros engine build guide (Phase 2/3 engine work)

Code-level how-to for the engine-side changes that make Micro-12 (Phase 2) and Micro-24 (Phase 3)
possible. Parallel to SIKIT_CLAUDE_CODE_GUIDE.md but for the ENGINE work rather than a specific
module -- the Micros' widget code is a separate spec (MONSOON_MICRO_CLAUDE_CODE_GUIDE.md); this is
what happens INSIDE the pitch/scale pipeline.

Read before: MICRO_TUNING_INTEGRATION_PLAN.md (the design), TWELVE_TET_AUDIT.md (the enumerated
12-TET hardcode locations), MONSOON_MICRO_SPEC.md (module semantics). This guide sits on top.

Rodney's caveat throughout: nothing released, nothing near release, working titles remain provisional.
This is a build recipe for when the time comes.

## What this guide covers vs what it doesn't

**Covers:**
- The shared `TuningTable` struct API (consumed by Sikit AND the Micros; owner rules; write ledger).
- Widening every 12-sized array to `MAXN=24` (the pervasive audit-and-widen work).
- Generalising the two engine seams (`pickSemitone` -> `pickDegree`; sem->voltage via cents).
- Rewriting the C/D-mode quantiser to use the tuning table.
- The staged build order that keeps behaviour byte-identical at 12-TET throughout.

**Does NOT cover:**
- Sikit's widget/panel/UI code -- see SIKIT_CLAUDE_CODE_GUIDE.md.
- The Micros' widget/panel/UI code -- see MONSOON_MICRO_CLAUDE_CODE_GUIDE.md.
- The Micros' .scl loading UI -- see SCALA_FILE_AND_LOAD_UI.md.
- Naming decisions -- see MICROTONAL_MASTER.md.

## The TuningTable struct API (concrete spec)

Location: `src/dsp/TuningTable.hpp` (header-only if it stays small; matching .cpp if it grows).
Namespace: `dotModular`.

```cpp
namespace dotModular {

// The shared tuning representation. All modes read from this; the tuning-authoring expander (Sikit,
// Micro-12, or Micro-24 -- one at a time per Monsoon) writes to it via the owner rules below.
struct TuningTable {
    static constexpr int MAXN = 24;

    int   N = 12;                    // Active degree count (12 by default; up to MAXN).
    float cents[MAXN];               // Per-degree cents from root (root = index 0 = 0 cents).
                                     // For N < MAXN, entries [N..MAXN-1] are undefined (don't read).
    float weight[MAXN];              // Per-degree probability weight, 0..1. 0 = disabled/skipped.

    // Convenience: reset to 12-TET defaults (called on Monsoon init and on tuning-source detach).
    void resetTo12TET() {
        N = 12;
        for (int i = 0; i < 12; ++i) {
            cents[i] = float(i) * 100.f;   // equal-division cents
            weight[i] = 0.f;               // Monsoon's scale system will populate from SEMI faders.
        }
        for (int i = 12; i < MAXN; ++i) {
            cents[i] = 0.f;
            weight[i] = 0.f;
        }
    }

    // For debug / assertion: is this table currently valid?
    bool isValid() const {
        if (N < 1 || N > MAXN) return false;
        if (cents[0] != 0.f) return false;  // Root must be 0.
        for (int i = 1; i < N; ++i) {
            if (cents[i] < 0.f || cents[i] > 1200.f) return false;
            if (weight[i] < 0.f || weight[i] > 1.f) return false;
        }
        return true;
    }
};

} // namespace dotModular
```

### Owner rules per field (the write-ledger discipline)

The TuningTable has THREE possible writer configurations, all single-owner per field per block:

| Tuning-source expander | `N` written by | `cents[]` written by | `weight[]` written by |
|------------------------|----------------|----------------------|-----------------------|
| None (built-in 12-TET) | Monsoon (=12)  | Monsoon (equal-div)  | Monsoon (from SEMI faders) |
| Sikit                  | Monsoon (=12)  | **Sikit**            | Monsoon (from SEMI faders) |
| Micro-12               | Monsoon (=12)  | **Micro-12**         | **Micro-12** (Monsoon SEMI faders blank) |
| Micro-24               | **Micro-24** (=24) | **Micro-24**     | **Micro-24** (Monsoon SEMI faders blank) |

Key discipline: EACH field has EXACTLY ONE writer per block. Never two Sikits, never Sikit + Micro,
never two Micros. Enforcement is on Monsoon's side via `claimAsTuningSource()` (see below); enforcement
of "which fields get written" is a matter of each expander only writing its own fields.

### Monsoon-side API for tuning-source arbitration

Add to `Monsoon` (or a helper it holds):

```cpp
class Monsoon {
public:
    // Called by any tuning-authoring expander (Sikit/Micro-12/Micro-24) in process().
    // Returns true iff this Monsoon accepts THIS expander as the tuning source for this block.
    // Enforcement: first found in the expander chain wins; rest return false (their ConnectMark greys).
    bool claimAsTuningSource(rack::engine::Module* expander);

    // Getter for expanders that want to know if they are the current source (for ConnectMark logic).
    rack::engine::Module* getTuningSourceExpander() const;

    // The shared table. Populated by whoever holds the claim, or reset to 12-TET when nobody claims.
    dotModular::TuningTable& getTuningTable();

    // Called by Monsoon at the top of each block, BEFORE process()es of any expanders. Resets the
    // per-block claim state (so the next block's first-claimer wins), and if no expander claimed last
    // block, ensures the table has been reset to 12-TET defaults.
    void refreshTuningSourceForBlock();
};
```

Implementation notes:
- `refreshTuningSourceForBlock` runs from Monsoon's `process()` early. Sets `tuningSource_ = nullptr`
  at block start; the first expander to call `claimAsTuningSource` wins.
- Expanders' `process()` runs AFTER Monsoon's? Or before? RACK EXPANDER ORDER: expanders run in
  their own process(), and Monsoon reads the shared message bus. In VCV Rack, expander modules'
  `process()` order isn't strictly defined -- but for this design, we don't NEED order-independence
  because the tuning table is consumed by MONSOON'S pitch generation, which happens in Monsoon's
  process. As long as expanders write BEFORE Monsoon reads, we're fine. Achieve this by having
  Monsoon read the tuning table LAST in its own process() (after its expander-message handling), and
  having each expander write in its OWN process() to a scratch area that Monsoon then reads.
- OR use Rack's expander message system directly: expander writes to `producerMessage`; Monsoon
  reads from `leftMessages[0]` / `rightMessages[0]`. Straits/Sands/Interchange all do this; copy the
  idiom. For the tuning table specifically, the message payload is small (TuningTable = ~200 bytes)
  so message-based communication is fine.
- Block-boundary delegation glitchlessness: when a Sikit/Micro attaches or detaches mid-run, the
  swap happens at the next `refreshTuningSourceForBlock` call -- clean, no pitch mid-block change.

### Where the TuningTable lives

It's owned by `Monsoon`. Its lifetime matches Monsoon's. Expanders (Sikit, Micros) DON'T own it --
they write into it via the arbitration mechanism above. Rationale: Monsoon is the module doing the
pitch generation; the table is fundamentally *Monsoon's* internal state, populated by whatever
tuning source is currently claimed.

## The pervasive widening: 12 -> MAXN=24

This is the mechanical audit-and-widen work. TWELVE_TET_AUDIT.md enumerates the classes; this guide
specifies the exact edits per class.

### Class-by-class widening list

For each class, the pattern is: `[12]` -> `[MAXN]` on arrays, `%12` -> `%N`, `*12` -> `*N` (with N
read from the TuningTable), `12.f` -> `float(N)`.

**`src/dsp/state/GateState.{hpp,cpp}`:**
- `semiPlayRemain[12]` -> `semiPlayRemain[dotModular::TuningTable::MAXN]`
- `lastSemitone` (int) -- stays int but semantic changes from 0..11 to 0..N-1. Add assertion
  `assert(semitone >= 0 && semitone < N)` in `triggerNote`.
- `triggerNote(..., int semitone, ...)` -- signature unchanged; caller now passes 0..N-1.

**`src/dsp/engines/PatternEngine.{hpp,cpp}`:**
- `semiWeights[12]` -> `semiWeights[MAXN]`
- `pickSemitone(const float weights[12], float r)` -> `pickDegree(const float weights[], int N, float r)`
  -- signature change; callers pass N.
- **THE VOLTAGE SEAM** (`genPitchLive`, line ~117 per integration plan):
  ```cpp
  // OLD:
  float v = float(oct) - 4.f + (sem + transpose) / 12.f;
  // NEW:
  float v = float(oct) - 4.f + tuningTable.cents[sem] / 1200.f;
  // (transpose is octave-only in Micro mode; see MICRO_TUNING_INTEGRATION_PLAN issue B)
  ```
  For 12-TET default (cents[i] = i*100), `cents[sem]/1200.f = (i*100)/1200.f = sem/12.f` -- so byte-
  identical to old. The regression test verifies.

**`src/dsp/engines/SequencerEngine.{hpp,cpp}`:**
- `activeSemiList[12]` -> `activeSemiList[MAXN]`
- `activeSemiWeight[12]` -> `activeSemiWeight[MAXN]`
- `faderCache[12]` -> `faderCache[MAXN]`
- `rebuildScaleCache(const float weights[12])` -> `rebuildScaleCache(const dotModular::TuningTable&)`
  -- signature change; reads `weight[]` internally.
- **THE C/D QUANTISER SEAMS** (lines ~957, ~968 per integration plan):
  ```cpp
  // OLD:
  int sem = int(std::round((pitchV - std::floor(pitchV)) * 12.f)) % 12;
  // NEW: nearest-enabled-degree by cents. Extract to helper for reuse in C and D.
  int sem = quantizeToNearestEnabledDegree(pitchV, tuningTable);
  ```
  Helper implementation:
  ```cpp
  int quantizeToNearestEnabledDegree(float pitchV, const dotModular::TuningTable& tt) {
      // Take octave-fractional part; convert to cents.
      float cents = (pitchV - std::floor(pitchV)) * 1200.f;
      int best = 0;
      float bestDist = std::numeric_limits<float>::max();
      for (int i = 0; i < tt.N; ++i) {
          if (tt.weight[i] <= 0.f) continue;  // disabled degree
          float dist = std::abs(cents - tt.cents[i]);
          // Wrap-around: also check dist to (cents[i] + 1200) and (cents[i] - 1200).
          dist = std::min(dist, std::abs(cents - (tt.cents[i] + 1200.f)));
          dist = std::min(dist, std::abs(cents - (tt.cents[i] - 1200.f)));
          if (dist < bestDist) { bestDist = dist; best = i; }
      }
      return best;
  }
  ```
  Edge case: if ALL degrees have weight 0 (whole scale disabled), fall back to nearest by cents
  regardless of weight, OR return -1 as a sentinel and let the caller pass through unchanged. Design
  decision (lean pass-through: silence is better than surprise). Flag at build time.

**`src/dsp/managers/MonsoonScaleManager.{hpp,cpp}`:**
- 12-bit scale mask -> N-bit scale mask stored in `TuningTable.weight[]` (where weight > 0 = enabled).
  This deprecates the standalone mask; the mask lives in the table now.
- `getSemitoneWeight(int semitone)` -> reads from `tuningTable.weight[semitone]`.
- Root selection (currently the "which fader is C" logic) -- degree 0 is always root by convention
  now. Root reassignment (Shophouse rotation) becomes a transform on cents[] rather than a bit-shift
  on the mask. Deferred design; note it here as a follow-up.

**`src/dsp/managers/MonsoonParameterManager.{hpp,cpp}`:**
- Per-lane pitch mod indices 0..11 -- widen to 0..N-1. Sentinels for octave-lo (currently 12) and
  octave-hi (currently 13) shift to N and N+1 respectively. Ensure sentinels are ALWAYS N and N+1,
  not the hardcoded 12 and 13, since N changes when Micro-24 attaches.
- Transpose -12..+12 -- see MICRO_TUNING_INTEGRATION_PLAN issue B (octave-only in Micro mode).

**`src/dsp/managers/MonsoonCVRouter.{hpp,cpp}`:**
- The "quantize CV to nearest semitone" logic -- reuse `quantizeToNearestEnabledDegree` from above.
  Not a separate quantiser.

**`src/Monsoon.{hpp,cpp}` + `src/MonsoonWidget.cpp`:**
- The 12 SEMI params (SEMI0..SEMI11) stay -- they're the built-in 12-TET scale faders, active when
  no Micro is attached. When a Micro-12 attaches, they BLANK (Monsoon-side param-quantity hidden,
  fader visually greyed). When Micro-24 attaches, same but fully blank -- the Micro owns everything.
  When Sikit attaches, they STAY LIT (Sikit only takes cents, not the mask).
- OCT_LO/OCT_HI params -- unchanged. Their SEMANTIC changes: octave-lo of `N` in a Micro-24 tuning
  is still 1V below root; the octave is still 1V. Only the internal division changes.

**`src/MonsoonShophouseExpander.cpp`:**
- `centres[12]` / `rects[12]` -- shutter click-zones for root select. Widen to `[MAXN]`. Panel
  layout: v1 stays 12-shutter (Shophouse is a Phase-1 module, not part of the Micros' design).
  Micro-24 users would use Shophouse for a subset-of-12 root selection, or Shophouse becomes N-aware
  in a later phase. Note as follow-up.

**`src/ScaleList.hpp` / `src/NoteValues.hpp`:**
- Scale interval tables + note names, 12-based. Widen the storage to `[MAXN]` but keep 12-TET presets
  intact. Add hooks for loading from `.scl` (via `dotModular::ScalaFile`). Deferred: adding non-12-TET
  presets like Slendro, Pelog, maqam-family, quarter-tone-EDO -- those come with content curation,
  not just code.

## Build order (Phase 2/3 engine work)

**Prerequisite: Phase 1 (Sikit) engine work is done.** Per SIKIT_CLAUDE_CODE_GUIDE.md build step 1,
the TuningTable already exists as an inert 12-TET default routed through Monsoon's pitch generation.
So the foundation is in place; the Phase 2/3 work is widening + full generalisation on top.

1. **Widen all `[12]` arrays to `[MAXN=24]`.** Pure structural change; behaviour still 12-TET because
   `N=12` at runtime. Tests stay green (30/30 baseline). Each class in the widening list touched, but
   no logic changes yet.

2. **Generalise `pickSemitone` -> `pickDegree(weights, N, r)`.** Callers pass N (from TuningTable).
   Still 12-TET behaviour because callers pass N=12. Verify tests still green.

3. **Generalise the voltage seam.** Change `(sem + transpose) / 12.f` -> `tuningTable.cents[sem] / 1200.f`.
   Byte-identical for 12-TET defaults (see arithmetic above). REGRESSION TEST at this step: with
   `TuningTable.cents[i] = i*100.f` and `TuningTable.weight[i] = SEMI_i_PARAM.value`, output must be
   byte-identical to pre-refactor. Fail = subtle bug in the arithmetic. This is the safety guarantee.

4. **Rewrite `rebuildScaleCache` to take TuningTable.** Signature change; callers pass the table.
   Weight[] provides the enable/disable + probability. Still 12-TET behaviour.

5. **Rewrite the C/D quantiser with `quantizeToNearestEnabledDegree` helper.** Ties to
   MODES_C_D_QUANTIZER_PRERELEASE.md (pre-release neglect pass -- coordinate). Test with 12-TET default:
   should match old quantiser output byte-for-byte. Test with sparse mask (e.g. only white keys
   enabled): should snap to nearest white key.

6. **Add Micro-12 arbitration.** Extend Monsoon's `claimAsTuningSource` to accept a Micro-12 alongside
   Sikit (mutually exclusive). Micro-12 writes both cents[] and weight[] (Sikit writes cents only).
   Monsoon's SEMI faders blank when a Micro (not a Sikit) claims.

7. **Verify Micro-12 with equal-division cents + all faders up == Monsoon standalone at 12-TET.**
   Automated regression test. This is the safety guarantee that Micro-12 introduces no drift.

8. **Sentinel-shift for Micro-24.** Wherever octave-lo=12 / octave-hi=13 sentinels are hardcoded,
   change to N / N+1 read from TuningTable. Test with N=12: identical behaviour. Test with N=24:
   sentinels correctly at 24/25.

9. **Add Micro-24 arbitration.** N=24 flag on the tuning table when Micro-24 claims. Writes cents[]
   and weight[] over the full 24 slots. Monsoon SEMI faders fully blank.

10. **Verify Micro-24 with N=24 equal-division cents + full mask == Monsoon standalone at some
    reference N=12 mapping?** This is the trickier regression -- N=12 and N=24 aren't the same
    output, so byte-identical isn't the right invariant. Instead: verify that N=24 with degrees
    0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22 enabled (every-other of a 24-EDO) at equal-division
    cents gives the same pitches as N=12 equal-division at 12-TET. Structural equivalence, not byte-
    identical.

11. **Pervasive audit for missed hardcodes.** `grep -rn "12\." src/dsp/` -- any remaining `12.f`s
    in pitch-relevant code that should be `float(N)` or `1200.f`? WriteLedger (see below) helps by
    catching them at runtime; also do a static pass.

12. **WriteLedger integration.** Add TuningTable to the write-ledger discipline
    (SequencerEngine.hpp:71+). Each field (`N`, `cents[]`, `weight[]`) has ONE writer per block;
    violations logged in debug. Same discipline that catches Macro-then-East StrandLedger conflicts.

Each step tests must pass before moving to the next. This is a two-week job realistically, not a
one-session job -- the audit is pervasive and every step needs Rack verification.

## Guard rails and safety invariants

- **The byte-identical 12-TET regression** at build step 3 is the anchor. Any refactor of the pitch
  path must preserve this at 12-TET defaults. If it ever breaks, the widening has introduced a subtle
  bug in the arithmetic and it must be fixed before continuing.
- **Structural-equivalence N=12 vs N=24** at build step 10 is a weaker but still useful invariant --
  N=24 with every-other degree enabled should sound like N=12 with all degrees enabled.
- **WriteLedger single-writer discipline** (build step 12) catches the "compiles clean, plausible
  wrong value" failure mode -- if two writers touch the same TuningTable field in one block, the
  ledger flags it in debug. This is the exact anti-pattern that bit us in the Macro-then-East
  situation.
- **No 12s left in pitch code.** Static audit at step 11 confirms every `12` in the pitch pipeline
  is either replaced by `N` (from table) or `1200.f` (cents-per-octave). Any residual `12.f` is a
  bug in waiting.
- **Assertion on TuningTable validity.** In debug builds, `assert(tuningTable.isValid())` before
  reading. Catches malformed writes.

## Known open questions (design decisions still needed at build time)

Some issues from MICRO_TUNING_INTEGRATION_PLAN remain flagged as decide-later. When Phase 2/3
building begins, these will need answers:

- **Issue D-follow-up: Lantern note-name display at N != 12.** Design lean was "degree numbers, no
  keyboard graphic for N != 12" but exact rendering was not spec'd. Coordinate with Lantern rework.
- **Colour-by-active-degree assignment model** (fixed-to-tuning-position vs assigned-over-enabled-
  subset) -- lean fixed-to-position but final call deferred.
- **Root reassignment under Shophouse for N != 12** -- currently a bit-shift on the 12-bit mask;
  in the tuning-table world it's a transform on `cents[]`. Design decision: does Shophouse remain
  12-only (Micro-24 users don't use Shophouse), or does Shophouse become N-aware?
- **`quantizeToNearestEnabledDegree` fallback when all weights are 0** -- pass-through or nearest-
  by-cents regardless. Lean pass-through but flag at build.

## Cross-refs

- MICRO_TUNING_INTEGRATION_PLAN.md -- the design; this guide implements it at code level.
- TWELVE_TET_AUDIT.md -- the enumerated hardcode locations; class-by-class widening list here
  matches its coverage.
- MONSOON_MICRO_SPEC.md -- module semantics (delegation rule, one-Micro-per-Monsoon, etc.).
- MONSOON_MICRO_CLAUDE_CODE_GUIDE.md -- widget/panel/UI code for the Micros; this guide covers
  the engine only.
- SIKIT_CLAUDE_CODE_GUIDE.md -- Sikit build guide; Phase 1's engine work (TuningTable inert 12-TET
  default) is the foundation this Phase 2/3 guide builds on.
- MODES_C_D_QUANTIZER_PRERELEASE.md -- pre-release C/D quantiser work; the `quantizeToNearestEnabledDegree`
  helper covers this after generalisation.
- SCALA_FILE_AND_LOAD_UI.md -- .scl / .kbm parsing; Micros consume via `dotModular::ScalaFile`.
- src/dsp/engines/PatternEngine.cpp:117 -- THE voltage seam.
- src/dsp/engines/SequencerEngine.cpp:957,968 -- THE C/D quantiser seams.
- SequencerEngine.hpp:71+ -- WriteLedger single-writer detector.
