# Philox key derivation fix + Change Alley external seed

Two related defects found while checking seed handling (Aug 2026). Both affect reproducibility and
both block the cross-instance / shared-seed work.

## Finding 1 (BUG): rhythm and melody Philox keys are IDENTICAL, not consecutive

### What the design says
The seed model recorded in design notes is three independent streams:
`rhythmKey = S`, `melodyKey = S + 1`, `caKey = S + 2`.

### What the code does
`PatternEngine.hpp:405-414`:
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
**Same derivation, no stream offset.** The two streams differ today only because `rhythmSeedFloat`
and `melodySeedFloat` usually hold different values.

### Why this is a real bug -- and it is the DEFAULT whenever the SEED input is used

There is only ONE `SEED_INPUT` jack (Monsoon.hpp:224). It feeds both streams via two calls to the
same sample-and-hold (Monsoon.cpp:325-327):
```cpp
if (inputs[SEED_INPUT].isConnected()) {
    engine.pe.setPendingRhythmSeed(sampleSeedFromSource());
    engine.pe.setPendingMelodySeed(sampleSeedFromSource());
}
```
`sampleSeedFromSource()` (Monsoon.cpp:343-354) reads that one jack and returns the clamped voltage --
the SAME value both times. So:

| SEED input | Seed floats | Philox keys | Result |
|---|---|---|---|
| **Patched** | identical | **identical** | **rhythm and melody are the SAME STREAM** |
| Unpatched | different (`rack::random::uniform()` drawn twice) | different | independent (works by accident) |

So the external-seed feature -- whose entire purpose is reproducibility -- is exactly the case that
collapses the two streams into one. Variation/legato/octave draws track rest draws. The unpatched
path only works because the fallback RNG happens to be called twice and returns different numbers.

This is not "a user might happen to set both seeds equal". **Using the SEED input at all triggers it.**

### The SEED jack has TWO read paths -- both collapse the same way
1. **Per-block** (ModeController.cpp:72-75): `seedSampleValue` is refreshed from the jack on EVERY
   process call while connected. Feeds the realtime-mode continuous reseed path (PatternEngine.hpp:53-58)
   -- a moving CV retunes the stream continuously there. Intentional.
2. **Per-gesture** (Monsoon.cpp:325-327): sampled when reseed-on-restart fires. The reproducible
   sample-and-hold path.

Both are deliberate designs. But note what this means for the bug: in path 2 the two
`sampleSeedFromSource()` calls are back-to-back in the SAME block reading the SAME jack, so they
**cannot** return different values -- the identical-key collapse is guaranteed, not merely likely. In
path 1, `seedSampleValue` is a single scalar shared by both streams, so the same collapse applies
wherever it is consumed.

### Implication for the fix: separate in the DERIVATION, not by re-reading
Because one voltage legitimately feeds both streams, per-stream independence must come from the key
derivation. The current double call to `sampleSeedFromSource()` is misleading -- it reads as though it
draws two values when structurally it cannot. Collapse it to ONE read and pass the stream index:
```cpp
if (inputs[SEED_INPUT].isConnected()) {
    const float s = sampleSeedFromSource();          // read ONCE
    engine.pe.setPendingRhythmSeed(s);               // deriveKey(s, STREAM_RHYTHM) downstream
    engine.pe.setPendingMelodySeed(s);               // deriveKey(s, STREAM_MELODY) downstream
}
```
Same for the per-block path: one `seedSampleValue`, stream separation applied at `deriveKey`.

Also note `seedRhythmPhiloxFull()` / `seedMelodyPhiloxFull()` (:415-416) each call
`rack::random::u64()` independently, so the FULL-reseed path is fine. Only the seed-float path collides.

### Fix
Add a per-stream offset to the derived key so identical seed floats still give independent streams:
```cpp
namespace { constexpr uint64_t STREAM_RHYTHM = 0, STREAM_MELODY = 1, STREAM_CA = 2; }

inline uint64_t deriveKey(float seedFloat, uint64_t stream) {
    float s = pe_clamp(seedFloat, 0.f, 10.f);
    uint64_t sd = (uint64_t)((double)s / 10.0 * (double)MAX_U64);
    return sd + stream;          // matches the documented S, S+1, S+2 model
}
```
`+ stream` is the literal reading of the design note. **Consider instead mixing the stream index
rather than adding it** -- adjacent keys in a weak PRNG can produce correlated streams. Philox is a
counter-based cipher and is specifically designed so adjacent keys decorrelate, so `+stream` is
defensible here; but a hash-mix (e.g. splitmix64 of `sd ^ (stream * GOLDEN)`) costs nothing and removes
the question entirely. **Lean: mix, not add.** Flag for Rodney -- if the documented `S, S+1, S+2`
model is meant literally for reproducibility across instances, keep `+`; if it's shorthand for
"three derived streams", mix.

### Reproducibility consequence
This changes the numbers produced by any given seed float. Existing saved patches will sound different
after the fix. That is acceptable pre-release (nothing shipped) but should be done BEFORE any release,
never after. Note it in the pre-release checklist.

## Finding 2 (MISSING FEATURE): Change Alley has no seed at all

### Current state
`MonsoonChangeAlleyV2.hpp:60-62`:
```cpp
uint64_t corrKey[CA::SIDES * CA::TYPES * 2] = {};
...
for (int i = 0; i < CA::SIDES * CA::TYPES * 2; ++i) corrKey[i] = rack::random::u64();
```
Keys are drawn from raw entropy at construction and persisted to JSON (:258-259, :277-280), so a
SAVED patch reproduces. But:
- No derivation from any seed value.
- **No SEED input jack.**
- No way to set the keys deterministically, or to share them across two CA instances.

### Why this blocks planned work
The cross-instance features depend on shared seeding:
- Shared-CA correlation across separate Monsoons (SEED_OFFSET_DESIGN, build order item 2).
- Cross-tuning canon / heterophony patches (PITCH_PATCHABILITY points 12, 12a, 12b) -- these need two
  Monsoons whose CA scatter is correlated, which requires the same `corrKey[]` in both.
- The documented `caKey = S + 2` model can't be implemented at all without a seed input.

### Fix: add a SEED input to Change Alley
Mirror Monsoon's existing seed handling so the idiom is consistent:
- **SEED input jack** (0..10V), plus `seedConnected` detection. PatternEngine already has this exact
  pattern (`seedConnected`, `seedSampleValue`, PatternEngine.hpp:61-62) -- copy it.
- When connected: derive all `corrKey[i]` from the CV via `deriveKey(seedCV, STREAM_CA + i)` so each
  of the 8 scatter streams still gets an independent key, but all are reproducible from one input.
- When disconnected: keep current behaviour (random at construction, persisted). Do NOT change the
  no-cable default -- patches that rely on it keep working.
- Consider a context-menu "reseed" action for deterministic re-randomisation without a cable.

### Sharing across instances
With a SEED input, two Change Alleys fed the same CV derive the same `corrKey[]` and therefore the same
scatter sequence -- which is exactly the mechanism the cross-instance patches need. Worth confirming at
build: same CV -> same keys -> same scatter, verified with two CAs side by side.

## Build order

1. **Add `deriveKey(seedFloat, stream)` helper** with the stream constants. One place, used by all.
2. **Migrate rhythm/melody to it** (STREAM_RHYTHM, STREAM_MELODY). Verify: same seed float in both now
   produces DIFFERENT streams. Test: set both seeds equal, assert rhythm and melody patterns differ.
3. **Add the SEED input to Change Alley** + `seedConnected` detection, mirroring PatternEngine's idiom.
4. **Derive `corrKey[]` from the seed when connected**, leaving the disconnected path unchanged.
5. **Rack-verify cross-instance**: two CAs, same SEED CV, confirm identical scatter. Two Monsoons with
   the same seed, confirm rhythm/melody/CA all reproduce.
6. **Regression test**: same seed float on rhythm and melody must produce independent sequences (this
   is the specific bug from Finding 1 -- encode it so it can't regress).
7. **Rack-verify the SEED-input case specifically**: patch a static CV into SEED, dice, and confirm
   rhythm and melody patterns are DIFFERENT. This is the exact scenario that is broken today, so it is
   the acceptance test for the fix. Then confirm the same CV value reproduces the same pair of patterns
   across a patch reload (reproducibility must survive the fix).

## Guard rails
- **Severity note**: this is not a latent edge case. Any patch using the SEED input today has rhythm
  and melody locked to the same stream. Treat as a real bug, not a polish item.
- The fix CHANGES what any given seed float produces. Pre-release only. Add to the pre-release
  checklist so it can't slip past a release.
- Don't change the no-cable CA default (random-at-construction, persisted) -- only add the connected path.
- `seedRhythmPhiloxFull()` / `seedMelodyPhiloxFull()` already use independent entropy; leave them alone.

## Cross-refs
- PatternEngine.hpp:405-416 -- the identical-derivation bug.
- PatternEngine.hpp:61-62 -- the `seedConnected` / `seedSampleValue` idiom to copy for CA.
- MonsoonChangeAlleyV2.hpp:60-62 -- CA key init; :258-259,:277-280 -- CA key persistence.
- SEED_OFFSET_DESIGN.md -- the offset feature; shared CA is build-order item 2 and depends on this.
- PITCH_PATCHABILITY_AND_DISTINCTION.md points 12/12a/12b -- cross-instance patches that need shared CA seeding.
- CA_DICE_COUNTER_MODEL.md -- the counter model (unaffected; this is about KEYS, not counters).
