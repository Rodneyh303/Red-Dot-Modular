# Scala (.scl) file class + load UI -- shared infrastructure

Reusable Scala-tuning-file parser + a small file-picker UI. Consumed by Sikit (Phase 1), Micro-12 (Phase 2), Micro-24 (Phase 3). Placed in shared code so each module wires the same parser and picker
rather than reimplementing.

## Why one shared class

Sikit, Micro-12, and Micro-24 all read .scl files. Same format, different constraints per module:
- Sikit: 12 degrees only. Reject other counts.
- Micro-12: 12 degrees only. Reject other counts.
- Micro-24: 1..24 degrees (any count up to MAXN). Reject counts >24.
The PARSING is identical; only the ACCEPT criterion varies. So parse once, provide a caller-supplied
predicate for accept/reject. One class, three callers, zero duplication.

## Location + naming

- `src/tuning/ScalaFile.hpp` (single header; parser is small enough not to need a .cpp).
- Header-only or a matching `.cpp` -- header-only is fine for a parser this size.
- Namespace: `dotModular` (matches the collection's shared-infrastructure namespace used by
  `dotModular::ConnectMark` and `dotModular::findMonsoonEitherSide`).

## The class

### Data structure
```cpp
namespace dotModular {

struct ScalaFile {
    // Parsed contents. All units are CENTS from root; root is implicit 0.
    std::string description;            // The .scl "description" line (first non-comment line).
    std::vector<float> centsFromRoot;   // Size = degree count. centsFromRoot[i] = degree (i+1)'s
                                        // cents from root. Root itself (0 cents) is NOT stored.
                                        // Typical: 12 entries for 12-note scales, last entry usually
                                        // 1200.f (the octave, though Scala allows stretched octaves).

    // Optional convenience: source path (populated by loadFromFile; empty for parsed-from-string).
    std::string sourcePath;

    // Parse status. If not Ok, `errorMessage` explains why.
    enum class Status { Ok, IoError, ParseError, RejectedByPredicate };
    Status status = Status::Ok;
    std::string errorMessage;   // Human-readable message, safe to show in a dialog.

    // Convenience predicate: was the load successful?
    bool ok() const { return status == Status::Ok; }
    int degreeCount() const { return (int)centsFromRoot.size(); }
};

} // namespace dotModular
```

### Parsing API
```cpp
namespace dotModular {

// Parse from an already-loaded string (e.g. contents of a .scl file).
// `acceptFn` is called with the parsed degree count BEFORE returning; if it returns false, the
// result's status is set to RejectedByPredicate and errorMessage is filled in from `rejectMessage`.
// This lets each caller enforce its own degree-count constraint without duplicating parse code.
ScalaFile parseScala(
    const std::string& text,
    std::function<bool(int degreeCount)> acceptFn = nullptr,
    const std::string& rejectMessage = "This tuning file has an unsupported degree count.");

// Load and parse from a file path. Wraps parseScala with file-IO error handling.
ScalaFile loadScala(
    const std::string& filePath,
    std::function<bool(int degreeCount)> acceptFn = nullptr,
    const std::string& rejectMessage = "This tuning file has an unsupported degree count.");

} // namespace dotModular
```

### Example callers
```cpp
// Sikit: EXACTLY 12 (Rodney's refinement)
// Sikit retunes Monsoon's fixed 12-degree system -- there is no natural mapping for a shorter file
// (e.g. a 7-note pentatonic has no obvious place among 12 cents knobs), so all-or-nothing on 12.
auto sf = dotModular::loadScala(path,
    [](int n){ return n == 12; },
    "Sikit reads exactly 12-note .scl files (it retunes Monsoon's 12-degree system). "
    "For scales with fewer or more degrees, use a Micro expander.");

// Micro-12: UP TO 12 (Rodney's refinement)
// Micro-12 authors both tuning and scale, so a shorter .scl is meaningful (7-note Slendro,
// 5-note pentatonic, etc.) -- populates the first N slots, disables the rest (weight=0).
auto sf = dotModular::loadScala(path,
    [](int n){ return n >= 1 && n <= 12; },
    "Micro-12 supports up to 12 tones per octave. For more, use Micro-24.");

// Micro-24: up to 24
auto sf = dotModular::loadScala(path,
    [](int n){ return n >= 1 && n <= 24; },
    "Micro-24 supports up to 24 tones per octave. This file has more.");
```

## .scl format reference

Standard Scala tuning file format (defined by the Scala project, widely supported):

```
! optional_filename.scl
!
Description string on this line
 12
!
 100.0
 200.0
 300.0
 400.0
 500.0
 600.0
 700.0
 800.0
 900.0
 1000.0
 1100.0
 2/1
```

Rules:
1. Lines starting with `!` are COMMENTS -- skip them entirely.
2. The first NON-COMMENT line is the DESCRIPTION (free-form string, can be empty).
3. The second NON-COMMENT line is the DEGREE COUNT (positive integer; anything else = parse error).
4. Following non-comment lines: one pitch per degree, in one of two formats:
   - **Cents**: contains a decimal point (`.`), e.g. `100.0`, `701.955`. Value is cents from root.
   - **Ratio**: `numerator/denominator` (both positive integers), e.g. `3/2`, `2/1`. Convert to cents
     via `1200.0f * log2f((float)num / (float)den)`.
5. Whitespace: pitches may have leading/trailing whitespace; strip before parsing.
6. Trailing content on pitch lines (after the value) is allowed and MUST be ignored (many Scala files
   annotate pitches inline, e.g. `701.955  ! perfect fifth`).
7. If fewer pitch lines are present than the degree count declares, that's a parse error.
8. Extra pitch lines beyond the degree count are ignored (or treated as parse error -- pick one; be
   consistent; recommendation: ignore silently, since some files pad).

Root pitch (degree 0) is implicit 0 cents, NOT stored in the file. The stored `centsFromRoot[]` array
matches: it holds degrees 1..N, not the root.

## Loading UI (context-menu integration)

### Where the file picker goes

Right-click on the module panel -> context menu appears. Add a "Load .scl..." item. When clicked,
open a native file picker (via VCV Rack's `osdialog`), read the selected file, parse, and apply.

### Rack's file picker API

Rack provides `osdialog_file(action, path, filename, filters)`:
```cpp
#include <osdialog.h>

// In the module's appendContextMenu override:
void SikitWidget::appendContextMenu(Menu* menu) {
    ModuleWidget::appendContextMenu(menu);
    menu->addChild(new MenuSeparator);
    menu->addChild(createMenuItem("Load .scl...", "", [this]() {
        this->openScalaFilePicker();
    }));
}

void SikitWidget::openScalaFilePicker() {
    osdialog_filters* filters = osdialog_filters_parse("Scala Tuning:scl");
    char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
    osdialog_filters_free(filters);
    if (!path) return;   // user cancelled

    std::string pathStr(path);
    std::free(path);

    // Parse with Sikit's 12-only constraint
    auto sf = dotModular::loadScala(pathStr,
        [](int n){ return n == 12; },
        "Sikit reads 12-note .scl files only. For non-12 tunings, use a Micro expander.");

    if (!sf.ok()) {
        // Show error dialog
        osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, sf.errorMessage.c_str());
        return;
    }

    // Apply: overwrite the module's cents params from sf.centsFromRoot
    Sikit* mod = dynamic_cast<Sikit*>(this->module);
    if (!mod) return;
    // sf.centsFromRoot[0] is degree 1's cents (from root). Root stays 0.
    for (int i = 0; i < 12 && i < (int)sf.centsFromRoot.size(); ++i) {
        // Degree i+1 maps to SIKIT_CENTS(i+1)_PARAM; root SIKIT_CENTS0_PARAM stays 0.
        // NOTE: our convention (per Sikit spec) is params 0..11 = degrees 0..11 with degree 0 = root.
        // Scala's file lists degrees 1..N. So param at index (i+1) gets sf.centsFromRoot[i].
        int paramIdx = SikitIds::SIKIT_CENTS0_PARAM + (i + 1);
        if (paramIdx <= SikitIds::SIKIT_CENTS11_PARAM) {
            mod->params[paramIdx].setValue(sf.centsFromRoot[i]);
        }
    }
    // Optionally: persist source path for reload-on-open, display description in menu, etc.
}
```

### Error message conventions

- Use `osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, msg)` for parse/reject errors -- native modal
  dialog, blocks until user acknowledges.
- The `sf.errorMessage` field should be a complete sentence, safe to display verbatim.
- For degree-count rejection, the message MUST tell the user what the module accepts AND what to use
  for other counts. Example: "Sikit reads 12-note .scl files only. For non-12 tunings, use a Micro
  expander (Phase 2/3, not yet available)."
- For I/O errors: "Could not read file: [path]. [system error]"
- For parse errors: "Could not parse .scl file: [reason]. See https://www.huygens-fokker.org/scala/scl_format.html for format details."

### Optional UX niceties (defer to v2 if time-pressed)

- **Show currently-loaded file name in the context menu**: after successful load, the "Load .scl..."
  item could show "Load .scl... (currently: well-tempered.scl)" with a "Clear" sub-action to revert
  to equal-division defaults.
- **Persist last-load path** in `dataToJson()` so the same file reloads on VCV restart. This means
  distributing a patch also carries the tuning reference (though the actual .scl file has to travel
  with it, since Rack patches don't embed external files).
- **"Save as .scl..."** for writing the current tuning back out. Sikit v1 says NO (see spec); v2 could
  add. When it does, use the same `ScalaFile` class with a `writeToFile(path)` method.

## Build order (for this shared infrastructure)

1. **ScalaFile struct + parseScala from string.** Test with hand-crafted string inputs covering:
   equal-division 12-TET, well-tempered, meantone, a ratio-based tuning like 3-limit just intonation,
   comment-heavy files, files with trailing pitch annotations, malformed files (missing degree count,
   wrong number of pitches, non-numeric junk). Unit tests in `test/`; no Rack dependency.

2. **loadScala from file path.** Adds file-IO wrapping around parseScala. Test with real .scl files
   from the Scala archive (https://www.huygens-fokker.org/docs/scales.zip has thousands).

3. **Sikit's context-menu wiring.** Add "Load .scl..." to Sikit's appendContextMenu. Rack-test:
   picker opens, valid 12-note file applies correctly, 24-note file shows clear rejection, cancel
   works, malformed file shows parse error.

4. **Micros' context-menu wiring (Phase 2/3, later).** Same pattern with different acceptFn. Should
   be a trivial addition once the Sikit wiring is proven.

## What to avoid

- **Do NOT parse .scl in each module separately.** The whole point of `ScalaFile` as a shared class
  is one parser used by three (or more) modules. If you find yourself pasting parse logic into a
  module's .cpp, stop and use the shared class instead.
- **Do NOT silently truncate/pad on degree-count mismatch.** All-or-nothing: either the file matches
  the module's constraint (accept fully) or it's rejected (with a clear error message). Silent
  partial-loading is the "compiles clean, returns a plausible wrong value" failure mode.
- **Do NOT invent a custom file format.** Scala .scl is the standard, universally supported. Any
  export functionality (Sikit v2 or later) writes standard .scl. Don't accumulate proprietary
  variants.
- **Do NOT block the audio thread on file I/O.** File loading happens on the UI thread (called from
  context-menu handlers). Applying the parsed cents to params is a param write, which is safe from
  the UI thread. Do not do file I/O in `process()` under any circumstances.
- **Do NOT trust the file's degree count without verifying the pitch count matches.** Some malformed
  .scl files declare "12" then provide 11 pitches. Verify pitchLines.size() == declaredDegreeCount
  before returning Ok status.

## Guard rails

- Unit tests for `parseScala` cover the specific format edge cases enumerated above. Tests are
  container-runnable (no Rack); part of `test/run_all.sh`.
- The `ScalaFile` class does not depend on Rack -- pure C++ with `<string>`, `<vector>`, `<sstream>`,
  `<cmath>`. This lets it be unit-tested without SDK and reused in other tools (a hypothetical .scl
  browser widget, a diagnostic dump).
- The load UI depends on `osdialog.h` (Rack-provided) -- lives in the widget's .hpp/.cpp, NOT in the
  parser. Keep parser and UI concerns separate.

## Cross-refs

- SIKIT_CLAUDE_CODE_GUIDE.md -- Sikit's build guide; the .scl load section refers here.
- MONSOON_MICRO_CLAUDE_CODE_GUIDE.md -- Micros' build guide; same reference.
- MONSOON_MICRO_SPEC.md -- Micro spec, notes .scl READ + WRITE as a feature.
- TUNING_EXPANDER_SPEC.md -- Sikit design spec.
- https://www.huygens-fokker.org/scala/scl_format.html -- official Scala .scl format documentation.
- https://www.huygens-fokker.org/docs/scales.zip -- Scala scale archive (thousands of real-world .scl
  files for testing).
