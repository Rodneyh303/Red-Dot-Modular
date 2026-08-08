# Sikit (Tuning Expander, microtonal Phase 1) — deferred polish

Sikit Phase 1 is complete and Rack-verified (default 12-TET byte-identical; cents detune shifts pitch;
`.scl` 12-only load with name in menu; ConnectMark tuning-source; attach/detach glitchless). These are
OPTIONAL polish items intentionally deferred — none blocks Phase 2/3. Revisit when convenient or on demand.

## Deferred (all low priority)
1. **Per-knob cents readout** — show each degree's current cents value (tooltip already gives it via the
   ParamQuantity " cents" unit; a small on-panel/near-knob readout is the extra). 8HP is tight, so a
   hover/near-knob draw rather than a permanent label.
2. **Flats/sharps context toggle** — v1 is sharps-only (C#, D#, …). Offer a menu toggle to show flats
   (Db, Eb, …). Labels only; no behaviour change. (Noted in SIKIT_CLAUDE_CODE_GUIDE §Layout.)
3. **`.scl` reload-on-open** — persist the loaded file PATH (not just the display name) in dataToJson,
   and re-load it on patch open so edited-on-disk tunings refresh. Caveat: the .scl must travel with the
   patch (Rack patches don't embed external files). Currently we persist the param VALUES (so the tuning
   is preserved) + the display NAME only — reload-from-path is the extra.
4. **`.scl` WRITE / "Save as .scl…"** — export the current cents as a standard .scl (SIKIT guide says v1
   = no write; add via a `ScalaFile::writeToFile` on the shared class when wanted).
5. **`.kbm` support** — SCALA_FILE_AND_LOAD_UI §.kbm: adds nothing for Sikit's exactly-12 case
   (one natural mapping). Explicitly NOT for Sikit; belongs to the Micros if at all.
6. **Widget-drawn label placement pass** — the wordmark/note-name/'0 root' offsets in
   `SikitLabels::draw()` were set without live rendering; nudge font sizes/offsets if anything reads
   tight once seen at more zooms/themes.

## Not-deferred design decisions already settled (context, do NOT redo)
- Root (C) LOCKED at 0 cents (Scalar rule): no knob + engine clamp. Rodney chose Option 1 (distinct
  locked "0/root" plate, not an inert knob).
- Loaded-.scl name is MENU-ONLY ("Loaded: …" line) — on-panel band removed (too tight on 8HP).
- Modes C/D stay 12-TET in Phase 1; global in.transpose stays 12-TET semitones; MAXN=24 arrays (N=12).

## Cross-refs
- src/Sikit.hpp / src/Sikit.cpp, panel_src/gen_sikit.py
- src/tuning/ScalaFile.hpp, src/tuning/TuningTable.hpp
- docs/design/TUNING_EXPANDER_SPEC.md, SIKIT_CLAUDE_CODE_GUIDE.md, SCALA_FILE_AND_LOAD_UI.md
