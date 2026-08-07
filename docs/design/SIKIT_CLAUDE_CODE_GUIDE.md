# Sikit -- Claude Code build guide (Phase 1 Tuning Expander)

Code-level how-to for building Sikit, the cents-only tuning expander. This sits ON TOP of
TUNING_EXPANDER_SPEC.md (semantics + design) and MICRO_TUNING_INTEGRATION_PLAN.md (engine integration).
Read those first; this guide is the coordinate-level implementation how-to.

Working title Sikit is provisional (Rodney: nothing released, nothing near release). If renamed before
shipping, replace throughout mechanically. The BUILD is the point of this doc; the NAME is a variable.

## What you're building (one paragraph)
Sikit is a small expander (~8-12HP) that attaches to Monsoon and RETUNES the existing 12-degree system
by writing per-degree CENTS values into the shared TuningTable. It does NOT touch the scale mask
(Monsoon's own scale faders keep their job) and does NOT support degree counts other than 12. Purpose:
let users load well-tempered/meantone/stretch/expressive-detune tunings via `.scl` files or manual cents
adjustment, while keeping their existing 12-tone scale library on Monsoon. This is the "retune 12-TET,
keep your scale" module -- the common-case microtonal use that ships alone as Phase 1 before the Micros.

## The lift-and-shift plan
Sikit is smaller than the Micros. There is nothing to lift from Monsoon's fader bank -- Sikit does NOT
have degree-weight faders (that's the Micros' job). Sikit adds NEW per-degree cents knobs. The
lift-and-shift here is the EXPANDER IDIOM, not the note-fader machinery.

### Expander idiom to copy (Straits header comment as reference)
Read `src/MonsoonStraitsExpander.hpp` (top comment). The template pattern for a dot.modular expander:
- Uses `namespace MonsoonIds` for any shared param/light IDs (reuses parent Monsoon's enums where
  meaningful; declares its own where the concept is expander-specific).
- Finds its parent Monsoon via `redDot::findMonsoonEitherSide(module)` from
  `src/ui/VisualExpanderHelpers.hpp`. This is the STANDARD walk pattern used by Intertropical, Sands,
  Change Alley, and every other host-attached expander in the collection. Do NOT invent a new lookup.
- Places `light_connect` SVG marker for the ConnectMark widget (see below).
- Any per-block state pushed to the parent Monsoon happens in `process()`; the parent reads it.

## NEW params -- 12 cents knobs

### Param IDs (Sikit's own enum, in its header)
```cpp
namespace SikitIds {
enum ParamIds {
    SIKIT_CENTS0_PARAM,   // C  (root, LOCKED to 0)
    SIKIT_CENTS1_PARAM,   // C#
    SIKIT_CENTS2_PARAM,   // D
    ...                    // and so on
    SIKIT_CENTS11_PARAM,  // B
    NUM_PARAMS
};
enum InputIds  { NUM_INPUTS = 0 };  // No CV inputs in v1
enum OutputIds { NUM_OUTPUTS = 0 }; // No outputs -- Sikit publishes via expander bus + shared TuningTable
enum LightIds  { NUM_LIGHTS  = 0 };
}
```

### Cents knob configuration (equal-division default, root locked at 0)
```cpp
for (int i = 0; i < 12; ++i) {
    float defCents = i * 100.f;   // equal-division default: 0, 100, 200, ..., 1100
    module->configParam(SIKIT_CENTS0_PARAM + i, 0.f, 1200.f, defCents,
                        "Cents (degree " + std::to_string(i) + ")", " cents");
}
```

### Root-locked-at-0 enforcement (Scalar's rule)
Two options; pick one:
- **UI-level**: mark the SIKIT_CENTS0_PARAM's ParamQuantity as read-only (setSmoothValue always 0 via a
  custom ParamQuantity subclass; or hide the knob's rack::widget interaction and render as a static
  label showing "0"). Cleanest.
- **Engine-level**: in `process()`, always overwrite `params[SIKIT_CENTS0_PARAM].getValue()` with 0.f
  before reading (belt-and-braces; guarantees the invariant even if a preset/scene tries to move it).

Recommend BOTH: UI hides the knob affordance (no accidental drag), engine clamps to 0 (belt-and-braces
against any external write path such as .scl-load or preset restore).

## Panel (`panel_src/gen_sikit.py`)

### Layout
- ~8-12HP module. Vertical layout works well: 12 knobs in a compact grid (e.g. 3 cols x 4 rows) OR one
  vertical column if the panel is tall. Design decision at panel time; both fit the HP budget.
- Panel wordmark "Sikit" in Barlow Black, dot.modular brand palette (Singapore red #d4001a for the dot
  glyph, gold #c8960c accent, dark #070707 background). Match the compact single-word lockup used by
  other single-word modules (Monsoon, Straits, Shophouse, Lantern).
- Note labels on each knob strip: C, C#, D, D#, E, F, F#, G, G#, A, A#, B (using standard sharps, or
  optionally offer flat variant in context menu -- v1: sharps only).
- Root C knob: rendered greyed / locked visual (no interactive knob, or a static "0¢" label). User sees
  it's not touchable, which matches the semantic constraint.
- SVG marker `light_connect` for the ConnectMark widget. Position it near the panel top or bottom
  edge, following the pattern of other expander panels.

### Template files to copy structure from
- `panel_src/gen_shophouse.py` or `panel_src/gen_straits.py` -- the closest small-expander panel
  generators. Copy their brand palette + panel-metadata idiom + light_connect marker placement.
- Do NOT invent a new brand palette. Reuse the constants (colours, typography) already defined for the
  collection.

## Widget code (`src/Sikit.hpp` + widget)

### The module class
```cpp
struct Sikit : rack::engine::Module {
    Sikit() {
        config(SikitIds::NUM_PARAMS, SikitIds::NUM_INPUTS,
               SikitIds::NUM_OUTPUTS, SikitIds::NUM_LIGHTS);
        for (int i = 0; i < 12; ++i) {
            float defCents = i * 100.f;
            configParam(SikitIds::SIKIT_CENTS0_PARAM + i, 0.f, 1200.f, defCents,
                        "Cents (degree " + std::to_string(i) + ")", " cents");
        }
    }
    void process(const ProcessArgs& args) override;

    // JSON persistence (for .scl-load state if it's stored beyond param values)
    json_t* dataToJson() override;
    void dataFromJson(json_t* root) override;
};
```

### The ConnectMark widget wiring
Sikit's panel widget adds a `redDot::ConnectMark` via the `light_connect` SVG marker. The mark's
`connected` callback returns TRUE iff this Sikit is CLAIMED by a parent Monsoon (i.e. is the
authoritative tuning-authoring expander for that Monsoon). See `src/ui/ConnectMark.hpp` for the API:
```cpp
ConnectMark* mark = /* placed at light_connect marker */;
mark->connected = [this]() {
    // Return true iff this Sikit is the tuning source for its parent Monsoon.
    Monsoon* mon = redDot::findMonsoonEitherSide(module);
    if (!mon) return false;
    // Parent Monsoon exposes which expander is currently the tuning-source claimant.
    // (This method to be added on Monsoon as part of the delegation-rule wiring.)
    return mon->getTuningSourceExpander() == module;
};
mark->lightTheme = [this]() { /* your theme predicate, per collection convention */ };
```

## Engine integration -- writing to the shared TuningTable

### Where the TuningTable lives
Per MICRO_TUNING_INTEGRATION_PLAN.md, the shared TuningTable lives on the engine:
```cpp
struct TuningTable {
    int   N;                  // 12 for Sikit; up to MAXN=24 later
    float cents[MAXN];        // per-degree cents from root (root=0)
    float weight[MAXN];       // per-degree weight (0 = disabled)
};
```
For Phase 1 (Sikit alone, no Micros yet), MAXN can stay 12 -- the array widening to 24 is Phase 3's
job. But structuring the table now with `N` as a field (even if always 12) is what makes Phase 2 and 3
land cleanly. Introduce the TuningTable as an INERT 12-TET default first (step 1 of build order), then
wire Sikit to write to it (step 3).

### Sikit is a PARTIAL writer -- cents only, not weight
The important discipline for Sikit: it writes ONLY `cents[0..11]` on the TuningTable. It does NOT
touch `weight[]`. Monsoon's own scale-fader machinery continues to populate `weight[]` from its own
SEMI*_PARAM values (or Shophouse when attached, per the existing scale system).

Owner rules for the TuningTable (WriteLedger territory -- Phase 1 spec):
- `cents[]`: written by whichever tuning-authoring expander is claimed (Sikit, or later a Micro), else
  populated with equal-division default `i * 100.f` by Monsoon itself.
- `weight[]`: written by Monsoon's scale system as it does today; the Micros will TAKE OVER weight[]
  when they attach (Phase 2/3), but that's later. Sikit never writes weight.

Single-writer per field, per block. Multiple Sikits would attempt to claim the tuning-source role; the
one-tuning-expander-per-Monsoon rule (see Delegation) prevents conflict.

### The publish mechanism (in Sikit::process)
Each block, Sikit finds its parent Monsoon and pushes cents[] into the shared TuningTable. Simplest
form:
```cpp
void Sikit::process(const ProcessArgs& args) {
    // Enforce root=0 (belt-and-braces alongside UI-level lock)
    params[SikitIds::SIKIT_CENTS0_PARAM].setValue(0.f);

    Monsoon* mon = redDot::findMonsoonEitherSide(this);
    if (!mon) return;   // Standalone: nothing to do; ConnectMark greys via its callback.

    // Attempt to claim as tuning source. Monsoon's tuningSourceClaim() returns true iff we're
    // the authoritative Sikit (first found wins; rest of the enforcement-of-single-owner discipline
    // lives on Monsoon, same as ConnectMark grey/bright semantics).
    if (!mon->claimAsTuningSource(this)) return;

    // Write cents[] into the shared TuningTable. Only cents; leave weight[] alone.
    TuningTable& tt = mon->getTuningTable();   // engine's shared table
    tt.N = 12;
    for (int i = 0; i < 12; ++i) {
        tt.cents[i] = params[SikitIds::SIKIT_CENTS0_PARAM + i].getValue();
    }
    // weight[] is NOT touched here. Monsoon's own scale system writes it.
}
```
Method names (`claimAsTuningSource`, `getTuningTable`, `getTuningSourceExpander`) are illustrative --
match them to whatever the engine-side integration (MICRO_TUNING_INTEGRATION_PLAN Phase 1) names them
when it lands. If those methods don't exist yet, coordinate: the engine-side TuningTable refactor is
either done BEFORE Sikit build starts, or done in the same session so both sides land together. Do
NOT build Sikit against a nonexistent engine API and stub it.

### Attach/detach timing (block boundary, no glitch)
Same as the delegation-rule concern in MICRO_TUNING_INTEGRATION_PLAN issue F: when Sikit attaches or
detaches mid-patch, do the swap at block boundary. Monsoon's `getTuningTable()` populates with the
current claimant's cents at the top of each block; the switch from built-in equal-division defaults to
Sikit-authored cents happens between blocks, not mid-block. No pitch glitch.

## `.scl` file reading

### 12-note constraint
Sikit reads 12-note `.scl` files ONLY. Reject or warn on files with any other degree count:
- Show a clear message: "Sikit reads 12-note .scl files only. This file has N degrees. For non-12
  tunings, use a Micro expander (available in future release)."
- Do NOT partial-load (don't take the first 12 degrees of a longer file, or pad a shorter one). All-
  or-nothing keeps the semantics honest.

### `.scl` format basics (Scala tuning file spec)
Standard `.scl` format:
- Lines starting with `!` are comments (ignore).
- First non-comment line: description string (display in context menu if useful; not required).
- Next line: integer degree count (must be 12 for Sikit; reject otherwise).
- Following lines: one pitch per degree, expressed either as:
  * A cents value (contains `.` -- floating-point cents)
  * A ratio like `3/2` (integer/integer -- convert to cents: `1200 * log2(num/den)`)
Root (degree 0) is implicitly 0 cents in Scala format; the file lists degrees 1..N. So a 12-note `.scl`
has 12 pitch values on 12 lines, mapping to Sikit's cents[1..11] plus the octave (usually 1200.f but
Scala files may specify a stretched octave -- accept as given, though Sikit's UI knobs are 0..1200).

### Loading behaviour
- User right-clicks Sikit panel -> "Load .scl..." -> file picker.
- On successful load: overwrite params[SIKIT_CENTS1..11] with the loaded cents values. Root stays 0.
- On rejection (wrong degree count / parse error): show error dialog, do NOT modify current state.
- Persist the loaded .scl path in `dataToJson()` if you want reload-on-open behaviour (optional v1).

### No `.scl` WRITE in v1
Sikit does NOT export .scl. If a user has dialed in a custom tuning on Sikit's knobs and wants to save
it, they save the VCV patch (params persist). Add .scl WRITE in v2 if wanted.

## Delegation rule (from TUNING_EXPANDER_SPEC + MONSOON_MICRO_SPEC)

### One tuning-authoring expander per Monsoon
Rule: Sikit OR Micro-12 OR Micro-24 attached, never more than one. Enforcement mirrors the Micros'
delegation:
- Monsoon scans left/right for tuning-authoring expanders each block.
- First found wins -- becomes the tuning source. Its `claimAsTuningSource(module)` returns true.
- Any additional Sikits (or Micros) return false from claim; their ConnectMark greys; their `process`
  early-returns without writing to the TuningTable. NOT ignored -- visibly not claimed.

### Sikit-specific: Monsoon's scale faders DO NOT blank when Sikit attaches
This is what makes Sikit different from the Micros. The Micros blank Monsoon's scale faders because
they TAKE OVER the scale (weight[]). Sikit only takes over cents; the scale stays with Monsoon, so
Monsoon's faders keep their normal behaviour. Do NOT add blanking logic for Sikit.

### Standalone Sikit (no Monsoon anywhere)
`findMonsoonEitherSide` returns nullptr -> ConnectMark greys, `process` early-returns. Panel remains
visible and interactive (user can adjust knobs, load .scl -- their work isn't lost if they later attach
to a Monsoon). Same idiom as every other expander in the collection.

## Build order (incremental, testable)

Each step is INDEPENDENTLY testable. Don't move to the next until the previous is green.

1. **TuningTable engine refactor (INERT 12-TET default).** Introduce `TuningTable{N, cents[], weight[]}`
   on the engine as a shared struct. Route Monsoon's existing pitch generation through it. Populate
   with defaults: N=12, cents[i]=i*100.f, weight[i]=SEMI*_PARAM value. BEHAVIOUR MUST BE IDENTICAL to
   pre-refactor -- 30/30 tests still green, byte-identical output at 12-TET defaults. NO Sikit yet;
   pure engine refactor. This is the foundation; DO IT FIRST.

2. **Sikit skeleton module + panel.** Empty Sikit module, panel with wordmark + ConnectMark marker,
   no params yet. Attach to Monsoon in Rack; verify expander bus detection (Monsoon sees a Sikit is
   present). ConnectMark lights when adjacent to Monsoon, greys otherwise. Panel renders correctly.

3. **12 cents params + configuration.** Add the SIKIT_CENTS0_PARAM..SIKIT_CENTS11_PARAM enum + the
   configParam loop with equal-division defaults + root-lock enforcement (UI + engine both). Verify
   params appear in VCV's context menu with correct ranges and defaults. Root knob is not
   interactive.

4. **Publish cents[] to TuningTable via expander bus.** Wire Sikit's process() to call
   `claimAsTuningSource` and write cents[] to the shared TuningTable. AT FIRST, verify by LOGGING on
   Monsoon's side: does Monsoon see the cents update as Sikit knobs move? Don't consume yet.

5. **Monsoon consumes the Sikit-authored cents.** Change Monsoon's pitch generation to read
   TuningTable.cents[] (which is now either equal-division defaults or Sikit-authored). Verify: with
   default cents, output is byte-identical to pre-refactor 12-TET. With Sikit knobs moved, pitch
   output shifts as expected.

6. **ConnectMark full delegation semantics.** Multiple Sikits attached: first-found's mark lights,
   rest grey. Standalone Sikit: mark greys. Move Sikit to a different rack position (adjacent to
   different Monsoon): mark updates correctly.

7. **.scl file reading.** Add context-menu "Load .scl..." action. Test with a well-tempered 12-note
   .scl, a meantone 12-note .scl. Test rejection with a non-12-note .scl (e.g. a maqam 24-tone file
   from Scala archive): verify clear error message.

8. **Attach/detach without glitch.** Rack-verify: attach Sikit mid-playback, does pitch shift
   glitchlessly at next block boundary? Detach: does Monsoon revert to 12-TET defaults glitchlessly?

9. **Regression test.** Automated test in `test/` that with Sikit's default cents (equal-division), the
   engine output is BYTE-IDENTICAL to Monsoon standalone at 12-TET. This is the safety guarantee that
   Sikit-with-defaults introduces no drift. Should pass automatically; failure means the TuningTable
   plumbing has a subtle bug.

## What to avoid

- **Do NOT add scale-mask controls to Sikit.** The whole point of the Sikit/Micros split is that Sikit
  is cents-only. If you find yourself wanting to add a "disable this degree" toggle to Sikit, stop --
  that's a Micros feature, and adding it here would collapse the split we deliberately established.
- **Do NOT support non-12-note .scl files.** Reject them clearly and point at the future Micros. Don't
  half-support (partial-load, truncate, pad) -- silent misbehaviour on tuning files is worse than a
  hard rejection.
- **Do NOT write to TuningTable.weight[].** Sikit is a partial writer. weight[] stays with Monsoon.
  A stray write to weight[] would corrupt the scale mask silently, which is the "compiles clean,
  returns a plausible wrong value" failure mode that's bit us before.
- **Do NOT build against a nonexistent engine API.** If step 1 (TuningTable refactor) hasn't happened
  when you start Sikit, either do step 1 first or coordinate landing them together. Stubbing
  `claimAsTuningSource` or `getTuningTable` and hoping the engine catches up later leads to drift.
- **Do NOT invent a new panel palette or brand mark.** Reuse the established dot.modular brand tokens
  (Barlow Black, red #d4001a, gold #c8960c, dark #070707) and the ConnectMark widget from
  `src/ui/ConnectMark.hpp`. Sikit must sit visually alongside Monsoon in the collection.
- **Do NOT invent a new parent-Monsoon walk.** Use `redDot::findMonsoonEitherSide` from
  `src/ui/VisualExpanderHelpers.hpp`. Every other expander uses it; consistency matters.

## Guard rails

- Braces balanced + all tests green after each build-order step (baseline 30/30 must not regress).
- Step-9 regression test: Sikit at defaults + Monsoon = byte-identical to Monsoon standalone at 12-TET.
  This test lives permanently in the suite and guards against future refactor drift.
- WriteLedger (SequencerEngine.hpp:71+) territory: when TuningTable becomes shared state, its writes
  belong in the ledger. cents[] has ONE writer (Sikit when claimed, else Monsoon's equal-division
  default); weight[] has ONE writer (Monsoon's scale system). Enforce single-writer per field.
- em-dash/unicode `str_replace` gotcha: match text structurally or use python if a commit hits it. Same
  discipline as elsewhere in this codebase.
- Rack-only verification (container can't compile SDK): block-boundary attach/detach glitchlessness,
  ConnectMark grey/bright semantics, .scl round-trip fidelity, panel wordmark rendering.

## Cross-refs

- SCALA_FILE_AND_LOAD_UI.md -- shared .scl parser + file-picker UI (Sikit, Micro-12, Micro-24 all use).
  Sikit calls `redDot::loadScala(path, [](int n){ return n==12; }, "Sikit reads 12-note only...")`.
- TUNING_EXPANDER_SPEC.md -- Sikit design spec (semantics, scenario, distinct-from-Micros).
- MICRO_TUNING_INTEGRATION_PLAN.md -- engine-side TuningTable refactor + seams (step 1 of build order).
- MONSOON_MICRO_SPEC.md -- Micro-12/24, Phase 2/3; delegation rule that Sikit's one-per-Monsoon rule
  is consistent with.
- MONSOON_MICRO_CLAUDE_CODE_GUIDE.md -- the parallel CC guide for the Micros. Structural template for
  this doc; both guides follow the same shape.
- MICROTONAL_MASTER.md -- entry point tying the microtonal work together.
- src/MonsoonStraitsExpander.hpp -- expander idiom template (top comment).
- src/ui/ConnectMark.hpp -- claim/reject visual indicator (dot.modular mark).
- src/ui/VisualExpanderHelpers.hpp -- `findMonsoonEitherSide` walk (standard parent-Monsoon lookup).
- src/dsp/managers/MonsoonConfigurator.cpp:32,35-36 -- reference for Monsoon's SEMI/OCT params (NOT
  lifted here; Sikit does not have degree-weight faders).
- panel_src/gen_shophouse.py or gen_straits.py -- small-expander panel generator templates.

## Status
Working title Sikit is provisional; naming is captured in MICROTONAL_MASTER as a Phase 1 candidate with
honest caveats. Nothing released, nothing near release -- treat this guide as an actionable BUILD
recipe when the time comes, and refine as the engine-side TuningTable refactor lands and reveals its
actual API shape.
