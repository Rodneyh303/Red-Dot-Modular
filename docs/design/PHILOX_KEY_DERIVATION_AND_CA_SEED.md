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

### The SEED jack has TWO read paths -- one is a bug, one is dead code
1. **Per-block** (ModeController.cpp:72-75): `seedSampleValue` is refreshed from the jack on EVERY
   process call while connected. The PatternEngine.hpp:53-58 comment describes this as feeding a
   "realtime-mode continuous reseed" path. BUT: `seedSampleValue` is only ever WRITTEN here and is
   NEVER CONSUMED downstream (verified: no other call site reads it in PatternEngine.cpp or
   SequencerEngine.cpp). This per-block read is DEAD CODE -- the continuous reseed machinery was
   either never built or got removed. The assignment is noise at best, misleading at worst.
   Rodney: "realtime mode even in continuous reseed is meant to be phrase boundary only. We might
   ditch continuous reseed." The dead-code finding supports this: there is no continuous reseed in
   the engine to ditch. The comment describes an intent; the intent was not implemented.
   ACTION: remove the `seedSampleValue` per-block sample. Keep the `seedConnected` bool (it IS read
   elsewhere, for the !seedConnected realtime path checks). Simplify ModeController.cpp:73-75 to just
   set `seedConnected`, drop the `sampleSeedFromSource()` call.

2. **Per-gesture** (Monsoon.cpp:325-327): sampled when reseed-on-restart fires. The reproducible
   sample-and-hold path. This is the one that matters and the one with the key-collapse bug.

The per-block path being dead code CLARIFIES the fix: it is simpler than previously stated. One
read path to fix (path 2), one dead assignment to remove (path 1), and the derivation fix covers all
remaining cases.

### The fix: collapse to one read, stream index separates in deriveKey
```cpp
// ModeController.cpp -- keep the bool, drop the dead per-block sample
currentPatternInput.seedConnected = sc;
// currentPatternInput.seedSampleValue = ...;  // REMOVE -- never consumed downstream

// Monsoon.cpp:325-327 -- one read, stream separation in deriveKey
if (inputs[SEED_INPUT].isConnected()) {
    const float s = sampleSeedFromSource();   // read ONCE
    engine.pe.setPendingRhythmSeed(s);        // deriveKey(s, STREAM_RHYTHM) inside
    engine.pe.setPendingMelodySeed(s);        // deriveKey(s, STREAM_MELODY) inside
}
```
One voltage, two derived-but-independent keys. The double call was misleading (reads as though drawing
two independent values when structurally it cannot) -- the collapse makes the code honest.

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

## Finding 2 (MISSING FEATURE): Change Alley has no seed -- and needs NONE OF ITS OWN

### Current state
`MonsoonChangeAlleyV2.hpp:60-62`:
```cpp
uint64_t corrKey[CA::SIDES * CA::TYPES * 2] = {};
...
for (int i = 0; i < CA::SIDES * CA::TYPES * 2; ++i) corrKey[i] = rack::random::u64();
```
Keys are drawn from raw entropy at construction, persisted to JSON. No derivation from any seed value.
`resetToIdentity()` (verified in code, MonsoonChangeAlleyV2.hpp:246) re-keys internally -- the comment
even says "external-seed sharing TBD", which is now decided here.

### Why this blocks planned work
The cross-instance features depend on shared seeding:
- Shared-CA correlation across separate Monsoons (SEED_OFFSET_DESIGN, build order item 2).
- Cross-tuning canon / heterophony patches (PITCH_PATCHABILITY 12/12a/12b).
- The documented `caKey = S + 2` model can't be implemented without seed derivation.

### Architecture decision (Rodney): NO separate SEED input on CA
CA should get its seed FROM MONSOON at the moment of reset+reseed. No new jack needed. Reasons:
- Monsoon ALREADY owns the "when" of CA's state changes -- `MonsoonExpanderManager` calls
  `ca->applyPendingTransforms()` at phrase boundaries, and `resetToIdentity()` is also called from
  the Monsoon side. So Monsoon can trivially also pass the seed value when it triggers CA's reset.
- One seed value (from Monsoon's sampleSeedFromSource()) already produces rhythm + melody keys.
  Passing the SAME value to CA means ALL THREE stream families (rhythm, melody, CA scatter) derive
  from ONE source. Clean. No extra jack, no user coordination.
- Multi-instance ("master Monsoon"): CA is adjacent to ONE Monsoon in the expander chain.
  `findMonsoonEitherSide` returns that adjacent Monsoon. The same Monsoon that calls
  `applyPendingTransforms()` (the owner Monsoon, already established) is the seed authority.
  When CA is shared across two Monsoons, only the adjacent owner calls the reseed -- same owner
  model used everywhere. No extra configuration.

### Fix: Monsoon passes seed value to CA on reset+reseed
On Monsoon's reset+reseed gesture, `MonsoonExpanderManager` (or wherever `resetToIdentity()` is
called) adds a `ca->reseedCorrKeys(seedValue)` call alongside the existing reset logic:

```cpp
// In MonsoonExpanderManager or equivalent reset path
// (same place that calls ca->applyPendingTransforms() / resetToIdentity()):
void onMonsoonReseedCA(MonsoonChangeAlleyV2* ca, float seedValue) {
    for (int i = 0; i < CA::SIDES * CA::TYPES * 2; ++i) {
        ca->corrKey[i] = deriveKey(seedValue, STREAM_CA + i);
    }
}
```

The `seedValue` is the SAME `s` from `sampleSeedFromSource()` that seeds rhythm and melody --
one read, three stream families:
```cpp
const float s = sampleSeedFromSource();   // ONE read
engine.pe.setPendingRhythmSeed(s);        // deriveKey(s, STREAM_RHYTHM)
engine.pe.setPendingMelodySeed(s);        // deriveKey(s, STREAM_MELODY)
if (ca) onMonsoonReseedCA(ca, s);         // deriveKey(s, STREAM_CA + i)
```

Construction-time init stays as `rack::random::u64()` (no seed known yet; entropy is correct
at construction). The seed derivation fires only on an explicit reset+reseed gesture, at which point
the seed value IS known. Persisted corrKey[] already handles patch-save reproducibility.

### CODE FIX NEEDED in resetToIdentity() (Rodney's ruling: reset != reseed)
`resetToIdentity()` (MonsoonChangeAlleyV2.hpp:240-244) currently calls `seedCorrKeysInternal()`:
```cpp
void resetToIdentity() {
    for (int v = 0; v < CA::N_VOICES; ++v) { rhythmSrc[v] = v; melodySrc[v] = v; }
    for (int i = 0; i < CA::SIDES * CA::TYPES * 2; ++i) scatterCounter[i] = 0;
    seedCorrKeysInternal();   // <-- WRONG: should not be here
}
```
**These are two distinct operations that must not be conflated:**
- `resetToIdentity()` = structural reset. Returns the pin matrix to identity (each voice maps to
  itself) and zeros the scatter counters (return to position 0 in the current stream). Does NOT
  change which stream you're on. A user may want to reset the matrix without changing their scatter
  keys.
- Reseeding = changes the Philox keys (which stream you draw from). Separate decision, triggered
  by an explicit seed gesture from Monsoon. If keys changed on every matrix reset, scatter streams
  would silently change in unexpected places.

**Fix:** remove `seedCorrKeysInternal()` from `resetToIdentity()`. The matrix reset and counter
zero stay; the key derivation is removed. Key derivation is called ONLY from `onMonsoonReseedCA()`
on explicit reset+reseed gesture (as specified above).
```cpp
void resetToIdentity() {
    for (int v = 0; v < CA::N_VOICES; ++v) { rhythmSrc[v] = v; melodySrc[v] = v; }
    for (int i = 0; i < CA::SIDES * CA::TYPES * 2; ++i) scatterCounter[i] = 0;
    // NO key re-derivation here. Keys change only on explicit reseed gesture.
}
```
Construction-time key init (line 62: `corrKey[i] = rack::random::u64()`) stays -- that is correct
(no seed known at construction, entropy is appropriate). Only the `resetToIdentity()` call to
`seedCorrKeysInternal()` is removed.

### Cross-instance (multi-Monsoon sharing one CA)
The adjacent (owner) Monsoon provides the seed on its reset+reseed. The non-adjacent Monsoon
doesn't call into CA (it's not adjacent). So if you want both Monsoons to share CA with the same
scatter, give them the same SEED CV and reset them together. No new protocol; just the existing
reset ownership model applied to seed derivation.

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
