# Monsoon Micro 12 + Monsoon Micro 24 -- Claude Code build guide

Build guide for the two tuning-authoring expanders. This is a "lift-and-shift with per-fader cents"
task, NOT a from-scratch design. Read MONSOON_MICRO_SPEC.md (semantics) and
MICRO_TUNING_INTEGRATION_PLAN.md (engine integration) FIRST; this guide is the code-level how-to that
sits on top of them.

## What you're building (one paragraph)
Two expanders (Micro-12, Micro-24) that AUTHOR a tuning table (per-degree probability + per-degree cents)
and, when attached to Monsoon, DELEGATE the tuning from Monsoon to themselves (Monsoon's own note faders +
octave-range knobs BLANK OUT). The Micro's faders REPLACE Monsoon's 12 semitone faders (Micro-12 = same
count, Micro-24 = doubled), each gaining a CENTS KNOB neighbour for detune (equal-division default, drag
to detune -- Scalar-style). Faders set NOTE PROBABILITY (the scale mask + weights); cents set the TUNING
(where each degree sounds).

## The lift-and-shift plan
The core structure copies Monsoon's note/octave section 1:1 (12) or doubled (24). Then per-fader cents
knobs are added -- that's the only genuinely new UI element.

### From Monsoon, LIFT these exact params (MonsoonConfigurator.cpp:32,35-36; Monsoon.hpp:~90-94)
- SEMI0_PARAM ... SEMI11_PARAM: 12 faders, 0.f..1.f, default varies per semitone. This is the "Semitone
  weight" bank. Copy the config idiom verbatim.
- OCT_LO_PARAM: 0..8, default 2. Lowest octave.
- OCT_HI_PARAM: 0..8, default 5. Highest octave.
- EXPANDER_OCT_LO_CV_INPUT / EXPANDER_OCT_HI_CV_INPUT (Monsoon.hpp:318-319): CV inputs for octave range,
  ALREADY DEFINED as expander-facing. Reuse the same names/idiom on the Micro if the Micro wants CV in.

### For Micro-24: DOUBLE the faders + relabel the DEGREE space
- 24 fader params (Micro-only enum, e.g. MICRO_SEMI0_PARAM..MICRO_SEMI23_PARAM), same 0..1 range + config
  as SEMI*_PARAM. Layout: one row of 24 (see MONSOON_MICRO_SPEC "MICRO-24 LAYOUT: ONE ROW of 24").
- The 24 faders are DEGREES, not semitones. Label them by degree number (1..24), not note names --
  arbitrary Scala tunings don't have note names (see MICRO_TUNING_INTEGRATION_PLAN issue H).
- Octave range: same 2 params as Monsoon (OCT_LO/HI), no change.

### NEW per-fader CENTS knob (the only truly new UI)
- One cents knob per fader. N cents knobs (12 or 24). Config:
    configParam(MICRO_CENTS0_PARAM + i, 0.f, 1200.f, defaultCentsFor(i, N), "Degree %d cents", " cents");
- DEFAULT = equal-division cents: defaultCentsFor(i, N) = i * (1200.f / N). So Micro-12 defaults to
  0/100/200/.../1100 (12-TET); Micro-24 defaults to 0/50/100/.../1150 (24-EDO). Drag to detune.
- Layout: cents knob sits IMMEDIATELY next to its fader (paired strip: fader + cents knob together, so
  the user sees a degree as one unit). See MONSOON_MICRO_SPEC "Per-degree strip".
- CONSTRAINT: root degree (index 0) is ALWAYS 0 cents and NOT editable (Scalar's rule). Enforce via
  paramQuantities readonly or by clamping input to 0 for index 0.

### Enable/disable toggle per degree
- The fader IS the enable/disable (fader value == 0 -> disabled). No separate toggle needed. Matches how
  MonsoonScaleManager already treats zero weights as disabled. Keep it consistent with Monsoon's
  existing model -- don't add a separate boolean.
- Detent/snap: consider snap-to-zero at fader bottom for clean "disabled" positioning (Rodney to confirm
  at build; not blocking).

## The TUNING TABLE plumbing (from MICRO_TUNING_INTEGRATION_PLAN)
This is where the Micro FEEDS Monsoon. Read that plan for the full seams; here's the expander-side view.

The Micro publishes a TuningTable{N, cents[MAXN=24], weight[MAXN]}:
- N = 12 for Micro-12, 24 for Micro-24.
- cents[i] = the cents param value (equal-division default, drag to detune).
- weight[i] = the fader value 0..1 (0 = disabled).
- Root: cents[0] = 0 always.

Publishing mechanism: use the expander bus (LEFT/RIGHT expander messages) -- see the Straits/Causeway
poly expander idiom for the pattern (MonsoonStraitsExpander.hpp header comment: "reuses existing
MonsoonIds:: enums, no new engine plumbing -- panel + I/O simplification"). The Micro pushes its
TuningTable each block into the expander message; Monsoon reads it and uses it as its tuning source
INSTEAD of the built-in 12-TET (built-in remains the fallback when no Micro is attached).

## The DELEGATION RULE (from MONSOON_MICRO_SPEC)
- Only ONE Micro attached at a time. If both a Micro-12 and Micro-24 are somehow attached, the FIRST
  found in the expander chain wins; the other is ignored (or error-lit -- Rodney to decide).
- When a Micro IS attached: Monsoon's SEMI*_PARAM faders BLANK OUT on the panel + become inert
  (paramQuantities marked as hidden/greyed). Tone authority moves to the Micro.
- When Micro DETACHES: Monsoon's own SEMI* faders re-activate, Monsoon reverts to its built-in 12-TET
  table. Do this at a block boundary (not mid-block) to avoid a pitch glitch (see
  MICRO_TUNING_INTEGRATION_PLAN issue F).
- Micro-12 attached: Monsoon's 12 faders blank 1:1.
- Micro-24 attached: Monsoon's 12 faders blank ENTIRELY (Monsoon has 12 slots, Micro owns 24 degrees
  with no correspondence). Same visual signal (blank), no 1:1 mapping needed.

## Panel source (panel_src) -- templates
- START FROM: panel_src/embed_monsoon.py (line 6 is the SEMI*_PARAM row config, line 7 is OCT_LO/HI).
  This is the exact section you're lifting.
- CLOSEST fader-heavy expander pattern: panel_src/gen_shophouse.py or gen_straits.py -- expander idiom
  (rack width, panel colours matching the Monsoon palette).
- Create panel_src/gen_monsoon_micro_12.py and panel_src/gen_monsoon_micro_24.py.
- Micro-12: 12 fader-cents strips in one row. ~15-18HP.
- Micro-24: 24 fader-cents strips in one row (see spec "ONE ROW of 24"). Wide -- ~30-36HP. Layout: each
  strip = fader + cents knob + degree label. Vertical strips, degree number per strip.

## VCV Scalar functions to adopt (from https://vcvrack.com/Scalar)
Adopt these (already noted in MONSOON_MICRO_SPEC):
- Equal-division cents DEFAULT (Scalar's default). DONE via defaultCentsFor(i, N) above.
- Per-degree CENTS param (Scalar has one dial + selection; we do per-fader because we have room). DONE.
- Enable/disable per degree (Scalar: click to toggle). We do fader = 0. DONE.
- Scala .scl IMPORT / EXPORT (context menu, right-click, per MONSOON_MICRO_SPEC "Scala .scl READ+WRITE").
  Adopt Scalar's format faithfully; .scl is standard. Root always 0 cents. Save = write cents[] + which
  degrees are enabled (weight > 0) back to a .scl. Load = populate cents[] + set weights to non-zero for
  imported degrees (weight can default to a mid value; user adjusts).
Do NOT adopt from Scalar:
- OCTAVES per-octave enable/disable variability -- REJECTED (see MONSOON_MICRO_SPEC "OCTAVE-INVARIANT").
- Humanization (depth/track/warp/offset) -- skip for v1 (may revisit).
- One shared cents dial + selection -- rejected (we have room, per-fader is better).

## Build order (incremental, testable)
1. Skeleton Micro-12 module + panel (empty, no fader logic yet). Attach to Monsoon, verify expander bus
   detection works (Monsoon sees it). This is the plumbing sanity check.
2. Add the 12 fader params + config (lift-and-shift from Monsoon SEMI*_PARAM). Verify they appear on the
   panel and read/write via VCV context menu.
3. Add the 12 cents knobs with equal-division default + root-is-0-and-locked. Verify defaults are 0/100
   /200/.../1100.
4. Wire the expander bus: Micro publishes TuningTable, Monsoon receives. AT FIRST, Monsoon just LOGS the
   received table -- doesn't use it yet. Verify Monsoon sees the table update as Micro faders/cents move.
5. Wire the delegation-blanking on Monsoon: when Micro attached, Monsoon's SEMI* faders blank. When
   detached, they reactivate. Verify at block boundary (no glitch).
6. Wire consumption: Monsoon switches its tuning source from built-in 12-TET to Micro's table (this is
   the MICRO_TUNING_INTEGRATION_PLAN work -- do that FIRST or in lockstep; the guide there is the
   engine-side plumbing).
7. .scl import + export (context menu).
8. Micro-24: repeat with N=24, one row of 24, defaultCentsFor(i, 24). Same code path, N-parameterised
   from step 2 onward.
9. Tests: engine tests for TuningTable population from Micro; regression test that N=12 Micro with
   default cents produces identical output to built-in 12-TET (same bytes, same behaviour -- the
   safety guarantee).

## What to avoid
- DO NOT add per-degree enable/disable buttons -- the fader IS the toggle (weight 0 = disabled). Adding a
  separate boolean creates a second source of truth for disabled-ness. Follow Monsoon's existing model.
- DO NOT try to quantize input CV -- the Micro is a TUNING-DEFINITION expander (feeds Monsoon's tuning
  table), NOT a standalone quantizer (MONSOON_MICRO_SPEC "Micro DEFINES tuning table for output"). Modes
  C/D (Monsoon's quantizer modes) will consume the same table -- that's a separate integration point,
  see MODES_C_D_QUANTIZER_PRERELEASE.md.
- DO NOT bake in 12+quarter-tone assumptions for Micro-24 (no primary/inflection split in panel/roll/
  colour -- 24 degrees are treated as N arbitrary degrees, consistent from panel down).
- DO NOT invent a new panel palette. Reuse the Monsoon panel colour tokens (dot.modular Barlow Black,
  Singapore red #d4001a, gold #c8960c, dark #070707 -- see the brand identity work). Panels must sit
  cohesively next to Monsoon.

## Guard rails
- Braces balanced + all tests green after each build-order step (30/30 baseline).
- 12-TET regression test (step 9): Micro-12 with default cents MUST produce byte-identical output to
  Monsoon standalone in 12-TET. This is the "no behaviour drift" bar.
- WriteLedger (SequencerEngine.hpp:71) -- when the tuning table becomes a shared state read by pitch gen
  + quantizer, the ledger's single-writer discipline applies. Micro is the writer when attached; Monsoon
  built-in when not. Never both simultaneously (the delegation rule).
- The em-dash / unicode gotcha for str_replace: match structurally / via python if any commit hits it.
- Rack-only verification: block-boundary delegation glitchless-ness, .scl round-trip fidelity, Micro-24
  panel width in a real rack.

## Cross-refs
- MONSOON_MICRO_SPEC.md -- the semantics (feature set, delegation rule, layout, per-fader cents decision,
  ONE-ROW-24 layout decision).
- MICRO_TUNING_INTEGRATION_PLAN.md -- the engine seams the Micro's tuning table plugs into (pickDegree,
  cents voltage map, quantizer rewrite, sem-array widening 12->24).
- MICROTONAL_MASTER.md -- entry point tying all of the above together.
- MODES_C_D_QUANTIZER_PRERELEASE.md -- quantizer modes that ALSO consume the shared tuning table.
- MonsoonStraitsExpander.hpp -- template comment showing the expander idiom to copy.
