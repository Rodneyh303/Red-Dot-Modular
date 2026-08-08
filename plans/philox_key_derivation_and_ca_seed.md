# Philox key derivation fix + Change Alley seed-from-Monsoon — implementation plan

> Source: [`PHILOX_KEY_DERIVATION_AND_CA_SEED.md`](../plugins/Melodicer/docs/design/PHILOX_KEY_DERIVATION_AND_CA_SEED.md:1)
> Two related reproducibility defects. All doc claims verified against the code (Aug 2026).
> **Pre-release only** — the fix changes what any given seed float produces.

---

## 0. Verified facts (code audit)

- **Finding 1 bug is real.** [`seedRhythmPhilox`](../plugins/Melodicer/src/dsp/engines/PatternEngine.hpp:405)
  and [`seedMelodyPhilox`](../plugins/Melodicer/src/dsp/engines/PatternEngine.hpp:410) use the
  *identical* derivation `sd = s/10 * MAX_U64` with **no stream offset**.
- **Double-read is real.** [`Monsoon.cpp:326-327`](../plugins/Melodicer/src/Monsoon.cpp:326) calls
  `setPendingRhythmSeed(sampleSeedFromSource())` then `setPendingMelodySeed(sampleSeedFromSource())`
  — two reads of the same jack, same value.
- **Dead per-block read is real.** `seedSampleValue` is written at
  [`PatternEngine.hpp:62`](../plugins/Melodicer/src/dsp/engines/PatternEngine.hpp:62) and
  [`MonsoonModeController.cpp:74`](../plugins/Melodicer/src/dsp/managers/MonsoonModeController.cpp:74)
  but **never read anywhere** (grep-confirmed 0 consumers). `seedConnected` IS read elsewhere — keep it.
- **CA keys are entropy-only.** [`MonsoonChangeAlleyV2.hpp:60-63`](../plugins/Melodicer/src/MonsoonChangeAlleyV2.hpp:60)
  `corrKey[]` + `seedCorrKeysInternal()` (raw `rack::random::u64()`), and
  [`resetToIdentity()`](../plugins/Melodicer/src/MonsoonChangeAlleyV2.hpp:240) wrongly calls
  `seedCorrKeysInternal()`.
- **No `deriveKey` helper exists yet** (grep-confirmed).
- **`MAX_U64`** is `PatternEngine`-local at [`PatternEngine.hpp:488`](../plugins/Melodicer/src/dsp/engines/PatternEngine.hpp:488).
- **Shared RNG home:** [`PhiloxRng.hpp`](../plugins/Melodicer/src/dsp/PhiloxRng.hpp:92) is header-only,
  no Rack SDK, and already included by BOTH `PatternEngine.hpp` (`../PhiloxRng.hpp`) and
  `ChangeAlleyTransforms.hpp`. **This is the natural home for a shared `deriveKey()`.**
- **CA reseed authority:** the Monsoon reset+reseed block is
  [`Monsoon.cpp:320-337`](../plugins/Melodicer/src/Monsoon.cpp:320); the CA module is reachable via
  `expanderManager.cachedChangeAlleyV2`. Its transforms are applied from
  [`MonsoonExpanderManager.cpp:112`](../plugins/Melodicer/src/dsp/managers/MonsoonExpanderManager.cpp:112).

### Contradiction resolved
The doc's **Build order items 3-4** ("Add the SEED input to Change Alley") contradict the later
**"Architecture decision (Rodney): NO separate SEED input on CA"**. Rodney's decision supersedes:
**no new jack** — CA gets its seed FROM MONSOON on the reset+reseed gesture. Build-order items 3-4
are stale; items 1-2, 6-7 remain valid.

---

## 1. Open decision (flagged for Rodney in the doc) — MIX vs ADD

`deriveKey` can separate streams by `sd + stream` (literal `S, S+1, S+2` model) or by a hash-mix
(`splitmix64(sd ^ stream*GOLDEN)`). The doc **leans mix** ("costs nothing, removes the question").
Both are reproducible across instances (same seed → same keys). `+stream` is defensible because
Philox decorrelates adjacent keys; mix removes all doubt. **This plan implements MIX** unless Rodney
wants the literal additive model. (Confirm before merging — it changes the produced numbers.)

---

## 2. The changes (ordered, each independently verifiable)

### CHANGE A — add shared `deriveKey()` + stream constants
File: [`PhiloxRng.hpp`](../plugins/Melodicer/src/dsp/PhiloxRng.hpp:1) (header-only, shared).
Add, in the file's namespace (near the top, after includes):
```cpp
namespace dotModular { namespace seed {
    constexpr uint64_t STREAM_RHYTHM = 0, STREAM_MELODY = 1, STREAM_CA = 2;
    // splitmix64 finaliser — decorrelates adjacent keys so identical seed floats
    // across streams give independent sequences.
    inline uint64_t mix64(uint64_t x) {
        x += 0x9E3779B97F4A7C15ULL;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
        return x ^ (x >> 31);
    }
    // seedFloat 0..10 -> 64-bit key for the given stream. Same float, different
    // stream -> independent key (fixes the identical-derivation bug).
    inline uint64_t deriveKey(float seedFloat, uint64_t stream) {
        float s = seedFloat < 0.f ? 0.f : (seedFloat > 10.f ? 10.f : seedFloat);
        uint64_t sd = (uint64_t)((double)s / 10.0 * (double)0xFFFFFFFFFFFFFFFFULL);
        return mix64(sd ^ (stream * 0x9E3779B97F4A7C15ULL));
    }
}} // namespace dotModular::seed
```
Self-contained (own clamp + MAX_U64 literal) so it has zero dependency on `PatternEngine`.

> If Rodney chooses ADD: replace the return with `return sd + stream;` and drop `mix64`.

### CHANGE B — migrate rhythm/melody seeding to `deriveKey`
File: [`PatternEngine.hpp:405-414`](../plugins/Melodicer/src/dsp/engines/PatternEngine.hpp:405).
```cpp
inline void seedRhythmPhilox(float seedFloat) {
    rhythmPhilox.seed64(dotModular::seed::deriveKey(seedFloat, dotModular::seed::STREAM_RHYTHM));
    rhythmDrawCtr = 0;
}
inline void seedMelodyPhilox(float seedFloat) {
    melodyPhilox.seed64(dotModular::seed::deriveKey(seedFloat, dotModular::seed::STREAM_MELODY));
    melodyDrawCtr = 0;
}
```
`seedRhythmPhiloxFull()` / `seedMelodyPhiloxFull()` (:415-416) **unchanged** (independent entropy already).

### CHANGE C — collapse the double SEED read to one
File: [`Monsoon.cpp:325-327`](../plugins/Melodicer/src/Monsoon.cpp:325).
```cpp
if (inputs[SEED_INPUT].isConnected()) {
    const float s = sampleSeedFromSource();   // read ONCE
    engine.pe.setPendingRhythmSeed(s);
    engine.pe.setPendingMelodySeed(s);
    if (expanderManager.cachedChangeAlleyV2)  // CHANGE F: same seed feeds CA
        expanderManager.cachedChangeAlleyV2->reseedCorrKeys(s);
}
```

### CHANGE D — remove the dead per-block `seedSampleValue` sample
File: [`MonsoonModeController.cpp:71-75`](../plugins/Melodicer/src/dsp/managers/MonsoonModeController.cpp:71).
```cpp
if (mainModule) {
    currentPatternInput.seedConnected = mainModule->inputs[MonsoonIds::SEED_INPUT].isConnected();
    // seedSampleValue removed: written here, never consumed downstream (dead continuous-reseed path).
}
```
Also remove the now-unused field
[`PatternEngine.hpp:62`](../plugins/Melodicer/src/dsp/engines/PatternEngine.hpp:62)
`float seedSampleValue = 0.f;` (keep `seedConnected` at :61). Grep for `seedSampleValue` first to
confirm zero other references before deleting the field.

### CHANGE E — CA: split reset from reseed (reset must NOT re-key)
File: [`MonsoonChangeAlleyV2.hpp:240-244`](../plugins/Melodicer/src/MonsoonChangeAlleyV2.hpp:240).
```cpp
void resetToIdentity() {
    for (int v = 0; v < CA::N_VOICES; ++v) { rhythmSrc[v] = v; melodySrc[v] = v; }
    for (int i = 0; i < CA::SIDES * CA::TYPES * 2; ++i) scatterCounter[i] = 0;
    // NO key re-derivation here. Keys change only on an explicit reseed gesture (reseedCorrKeys).
}
```
Construction-time `seedCorrKeysInternal()` at the ctor ([:116](../plugins/Melodicer/src/MonsoonChangeAlleyV2.hpp:116)
via `resetToIdentity()`) — see note below — and `dataFromJson`'s `resetToIdentity()` call
([:265](../plugins/Melodicer/src/MonsoonChangeAlleyV2.hpp:265)) currently rely on `resetToIdentity()`
seeding the keys. After this change they won't. Handle:
- **Constructor** ([:116](../plugins/Melodicer/src/MonsoonChangeAlleyV2.hpp:116)): call
  `seedCorrKeysInternal()` explicitly right after `resetToIdentity()` so a fresh module still gets
  entropy keys (no seed known at construction — entropy is correct).
- **`dataFromJson`** ([:264-283](../plugins/Melodicer/src/MonsoonChangeAlleyV2.hpp:264)): it calls
  `resetToIdentity()` then loads `corrKey[]` from JSON if present. For OLD patches with no `corrKey`
  key, add a fallback `seedCorrKeysInternal()` when the JSON lacks `corrKey` (so a loaded pre-fix
  patch still has valid keys instead of all-zero). Keeps save/load reproducibility intact.

### CHANGE F — CA: add `reseedCorrKeys(seedValue)`
File: [`MonsoonChangeAlleyV2.hpp`](../plugins/Melodicer/src/MonsoonChangeAlleyV2.hpp:61), next to
`seedCorrKeysInternal()`.
```cpp
// Derive all correlation keys from an external seed value (0..10) supplied by the
// adjacent (owner) Monsoon on its reset+reseed gesture. Same seed value that seeds
// rhythm + melody -> one source, three stream families. See PHILOX_KEY_DERIVATION_AND_CA_SEED.md.
void reseedCorrKeys(float seedValue) {
    for (int i = 0; i < CA::SIDES * CA::TYPES * 2; ++i)
        corrKey[i] = dotModular::seed::deriveKey(seedValue, dotModular::seed::STREAM_CA + (uint64_t)i);
}
```
Requires `#include "dsp/PhiloxRng.hpp"` in `MonsoonChangeAlleyV2.hpp` if not already transitively
present (it includes `ChangeAlleyTransforms.hpp` which includes `PhiloxRng.hpp` — verify; add a
direct include for clarity if needed).

Called from CHANGE C (SEED connected). **Unpatched path:** the Monsoon rhythm/melody unpatched case
uses FULL entropy (`setPending*ReseedRoll(full=true)`), NOT the lossy float. Mirror that for CA — in
the `else` branch of [`Monsoon.cpp:328-331`](../plugins/Melodicer/src/Monsoon.cpp:328) add:
```cpp
} else {
    engine.pe.setPendingRhythmReseedRoll(0.f, /*full=*/true);
    engine.pe.setPendingMelodyReseedRoll(0.f, /*full=*/true);
    if (expanderManager.cachedChangeAlleyV2)
        expanderManager.cachedChangeAlleyV2->seedCorrKeysInternal();  // full entropy, matches r/m
}
```
This keeps CA consistent with rhythm/melody: SEED patched → reproducible derived keys; unpatched →
fresh entropy.

---

## 3. Out of scope (explicitly NOT this session)
- **New SEED jack on Change Alley** — superseded by Rodney's "seed from Monsoon" decision.
- **Rack manual verification** (build-order items 5, 7; two-CA / two-Monsoon reproducibility) — needs
  the running app; do after the code lands.
- **Pre-release checklist entry** — noted here; add wherever that checklist lives.
- The deprecated [`MonsoonChangeAlleyExpander.hpp`](../plugins/Melodicer/src/deprecated/MonsoonChangeAlleyExpander.hpp:1)
  (V1) — leave untouched (deprecated).

---

## 4. Verification (this session)
1. Grep `seedSampleValue` → after CHANGE D, zero references remain.
2. Build the plugin (`make`) → clean compile.
3. **Regression intent** (build-order item 6): set rhythm and melody seed floats EQUAL (or use the
   SEED input) → rhythm and melody patterns must now DIFFER. Encodable as a unit test on
   `deriveKey(x, STREAM_RHYTHM) != deriveKey(x, STREAM_MELODY)` and that the two Philox streams
   produce different first draws.
4. Confirm old-patch load path: a patch with no `corrKey` JSON still gets valid (non-zero) keys.

---

## 5. Risk table
| Risk | Sev | Mitigation |
|------|-----|-----------|
| Produced numbers change (reproducibility break) | expected | Pre-release only; documented; add to checklist |
| `resetToIdentity` no longer keys → all-zero keys on ctor/old-load | med | CHANGE E explicitly re-adds `seedCorrKeysInternal()` at ctor + JSON-missing fallback |
| MIX chosen but Rodney wanted literal `S+1` | low | §1 decision gate before merge; one-line swap |
| `deriveKey` include cycle | low | `PhiloxRng.hpp` is standalone header-only; already shared |
| CA not adjacent when reseed fires (`cachedChangeAlleyV2==nullptr`) | none | Guarded by `if (cachedChangeAlleyV2)`; unpatched entropy path unaffected |

---

## 6. Acceptance criteria
- [ ] `deriveKey` + stream constants in `PhiloxRng.hpp`, used by rhythm/melody/CA.
- [ ] Same seed float → rhythm ≠ melody (bug fixed).
- [ ] Single SEED read per reseed gesture (no double sampling).
- [ ] `seedSampleValue` field + write removed; `seedConnected` retained.
- [ ] `resetToIdentity()` no longer re-keys; ctor + old-patch load still get valid keys.
- [ ] `reseedCorrKeys(s)` derives CA keys; wired to the Monsoon reset+reseed gesture (patched) with
      an entropy fallback (unpatched).
- [ ] Clean build.
