# BUG: shared Philox key across streams on the external-seed path

Rodney flagged; verified in code Aug 2026. Small fix, real audible consequence.

## The bug

`Monsoon.cpp:327-328` (the reseed-on-restart path):
```cpp
if (inputs[SEED_INPUT].isConnected()) {
    engine.pe.setPendingRhythmSeed(sampleSeedFromSource());
    engine.pe.setPendingMelodySeed(sampleSeedFromSource());
}
```

`sampleSeedFromSource()` (Monsoon.cpp) is a sample-and-hold read of SEED_INPUT when connected -- it
returns the SAME clamped voltage on both calls, with no advance between them.

Those floats then become Philox keys via `PatternEngine.hpp:405-414`:
```cpp
inline void seedRhythmPhilox(float seedFloat) {
    float s = pe_clamp(seedFloat, 0.f, 10.f);
    uint64_t sd = (uint64_t)((double)s / 10.0 * (double)MAX_U64);
    rhythmPhilox.seed64(sd); rhythmDrawCtr = 0;
}
inline void seedMelodyPhilox(float seedFloat) {
    float s = pe_clamp(seedFloat, 0.f, 10.f);
    uint64_t sd = (uint64_t)((double)s / 10.0 * (double)MAX_U64);   // IDENTICAL derivation
    melodyPhilox.seed64(sd); melodyDrawCtr = 0;
}
```

Same input float -> same `sd` -> **same Philox key for rhythm and melody** -> identical streams.
Rhythm and melody draws become correlated when they are supposed to be independent.

### Why it only bites on the EXTERNAL path
With SEED_INPUT unpatched, `sampleSeedFromSource()` falls through to `rack::random::uniform()`, which
returns a fresh value per call -- so the two keys differ. By accident, not by design, but they differ.
Hence the defect is invisible until someone patches the SEED input. That matches Rodney's read ("this
might be only from external source").

### Audible consequence
Rhythm and melody sharing a stream means their decisions move together -- rest placement and note
choice correlate in a way the design explicitly does not intend. The whole point of separately-keyed
streams (rhythmKey = S, melodyKey = S+1, caKey = S+2) is independence. This silently collapses it.

## The fix: offset in the KEY DERIVATION, not the call sites

Put the offset where the key is computed, so it holds for EVERY seed path -- current and future --
rather than depending on each caller remembering to offset. Callers keep passing the same user-facing
seed float; the streams separate internally.

```cpp
// Stream identity offsets. Each generative stream derives a DISTINCT key from the same
// user-facing seed value, so one seed produces independent streams rather than identical ones.
// See SEED_STREAM_KEY_OFFSET_FIX.md.
static constexpr uint64_t KEY_OFFSET_RHYTHM = 0;
static constexpr uint64_t KEY_OFFSET_MELODY = 1;
// CA offsets live on the Change Alley (its key is the CA's own) -- see below.

inline uint64_t keyFromSeedFloat(float seedFloat, uint64_t streamOffset) {
    float s = pe_clamp(seedFloat, 0.f, 10.f);
    uint64_t sd = (uint64_t)((double)s / 10.0 * (double)MAX_U64);
    return sd + streamOffset;      // see "additive vs hashed" below
}

inline void seedRhythmPhilox(float seedFloat) {
    rhythmPhilox.seed64(keyFromSeedFloat(seedFloat, KEY_OFFSET_RHYTHM));
    rhythmDrawCtr = 0;
}
inline void seedMelodyPhilox(float seedFloat) {
    melodyPhilox.seed64(keyFromSeedFloat(seedFloat, KEY_OFFSET_MELODY));
    melodyDrawCtr = 0;
}
```

### Additive vs hashed offset -- DECISION NEEDED
`sd + 1` gives a different key, but adjacent keys. Philox is a strong PRF so adjacent keys should
produce uncorrelated streams -- that is exactly what a counter-based PRF guarantees, and it is why
the counter-as-position model works in the first place. So additive is defensible.

BUT: `sd` is derived by scaling a float into the full u64 range, so adjacent SEED CV values already
map to keys differing by large amounts -- meaning `+1` is a much smaller perturbation than the seed
quantum. Two different SEED voltages will never collide, but it is worth being deliberate.

- **Option A (additive, recommended):** `sd + offset`. Simple, obvious, reversible, easy to reason
  about, and Philox's guarantees cover it.
- **Option B (hashed):** `splitmix64(sd ^ (offset * GOLDEN))` or similar. Maximal separation, but adds
  a hash dependency and makes the key derivation harder to reproduce by hand (matters if a user ever
  wants to predict/replicate a seed).
Lean A. Flag if any correlation is audible in testing.

## Change Alley -- VERIFIED SAFE today, but the same trap is documented as open

CA holds `corrKey[8]` (MonsoonChangeAlleyV2.hpp:60), and `seedCorrKeysInternal()` (:61-62) fills them
with `rack::random::u64()` per stream -- **eight genuinely independent keys. No collision bug today.**
They persist via JSON (:256-280), so patches stay reproducible.

The in-code comment at :55-56 already anticipates exactly this report:
> *"INTERNAL seeding = 8 INDEPENDENT random keys (different per stream, like dice's seed\*PhiloxFull).
> EXTERNAL-seed sharing is TBD (same open question as dice)."*

So: CA needs **no fix now**, but if it ever gains an external seed path (or shares Monsoon's), it hits
the identical trap -- and worse, it needs EIGHT distinct offsets, not one, because it has eight streams.

**Reserve the offset range now** so a future CA seed path cannot collide with rhythm/melody:
```cpp
static constexpr uint64_t KEY_OFFSET_RHYTHM   = 0;
static constexpr uint64_t KEY_OFFSET_MELODY   = 1;
static constexpr uint64_t KEY_OFFSET_CA_BASE  = 2;   // CA scatter stream i uses CA_BASE + i (2..9).
                                                     // Reserved even though CA seeds internally today.
```
Costs nothing now; prevents the exact bug recurring when the CA external-seed path is built.

## External seed path -- Rodney: "I think it also needs the external seed path added"

Worth clarifying scope at build time. Two readings:
1. **The existing path is buggy** (the shared-key issue above) -- confirmed, fix as specified.
2. **Some seed entry point does not exist yet** -- e.g. a seed input on the Change Alley, or a way to
   set the seed other than via reseed-on-restart. Check whether SEED_INPUT is honoured on all the
   paths it should be (dice-triggered reseed, CV-triggered, context-menu) or only on the
   reseed-on-restart path at Monsoon.cpp:321-334.

Reading 1 is verified. Reading 2 needs Rodney to confirm which entry point he means.

## Test

Container-runnable, no Rack needed:
1. Seed both streams from the SAME float (simulating a patched SEED input).
2. Draw N values from each stream.
3. **Assert the sequences DIFFER.** Pre-fix this test fails (sequences identical); post-fix it passes.
4. Also assert reproducibility: same seed float -> same rhythm sequence across runs, and same melody
   sequence across runs. The fix must not break determinism.

Add to `test/` and to `run_all.sh`. This is exactly the kind of "compiles clean, returns a plausible
wrong value" defect the test suite exists to catch -- the streams were valid, reproducible, and wrong.

## Guard rails
- Determinism must survive: a given SEED voltage must still produce the same patterns across runs and
  across save/load. The offsets are constants, so this holds -- but the test asserts it.
- Saved patches: changing key derivation CHANGES the patterns a given seed produces. Any patch saved
  with the old derivation will sound different after the fix. Acceptable pre-release (nothing shipped),
  but worth noting -- post-release this would need a migration.
- 30/30 tests green plus the new one.

## Cross-refs
- Monsoon.cpp:321-334 -- the reseed-on-restart path with the doubled `sampleSeedFromSource()` calls.
- Monsoon.cpp `sampleSeedFromSource()` -- sample-and-hold when SEED_INPUT connected.
- PatternEngine.hpp:405-414 -- `seedRhythmPhilox` / `seedMelodyPhilox`, the identical derivations.
- PatternEngine.hpp:415-416 -- `seedRhythmPhiloxFull` / `seedMelodyPhiloxFull` (use `rack::random::u64()`
  independently, so NOT affected by this bug -- confirm they stay unaffected).
- MonsoonChangeAlleyV2.hpp -- `corrKey[8]`, the CA stream keys.
- CA_DICE_COUNTER_MODEL.md -- the counter/key model this rests on.
