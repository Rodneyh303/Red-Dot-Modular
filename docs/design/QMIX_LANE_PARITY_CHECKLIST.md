# q-mix lane parity checklist — treat q-mix as a FIRST-CLASS lane, not a late wedge

**Directive (Rodney):** most dev went into sequencer mode; q-mix was only conceived while working on
quantiser mode, so it arrives last. Accent also arrived last and was done *inconsistently*, which cost
extra work and caused latent bugs. **q-mix must match the existing lane patterns everywhere — engine,
UI, design — and must not be "special" just because it was added last.** This doc is the parity
checklist: for each subsystem, follow the pattern the analogous lanes already use.

## q-mix's lane class (so we mirror the RIGHT lanes)
q-mix is a **poly, melody-side, spread lane** — its natural twins are MELODY and OCTAVE (they use the
melody stream; q-mix uses its own STREAM_SOURCE_SELECT). It is a full editor lane at **slot 2** with the
same complement (LEN/OFF/ROT + CV/attens + spread + direction + prob-out + per-step). It is **not**
rhythm-side (rest/accent) and **not** a mono-only lane (variation/legato have no poly-lane id). Concrete
ids it joins: strand `STRAND_QMIX`, poly-lane `PL_QMIX`, RNG stream `STREAM_SOURCE_SELECT` (reserved).

## The accent anti-pattern — what NOT to repeat (evidence, file:line)
- `SequencerEngine.cpp:110` — `for (l < PL_LANES) // was l < 3 — PL_ACCENT(3) was never reset (pre-existing bug)`.
  A hardcoded loop bound didn't scale when accent became the 4th poly lane → its state silently never reset.
- `Monsoon.hpp:170-173` — the accent GLOBAL_*_DNA block was inserted **between** octave's len/off/rot and
  `GLOBAL_OCTAVE_INTERP`, splitting octave's own param group.
- `Monsoon.hpp:81,221,237` — `// New: accent …` wedges (`ACCENT_KNOB`, `ACCENT_CV_INPUT`, `DNA_A_*`) appended
  ad-hoc rather than following a uniform per-lane block; the `DNA_*_INPUT` order is a historical accretion
  (R,V,L,A,M,O), not the editor order.
- `SequencerEngine.hpp:298` **and** `PatternEngine.hpp:108` — the `PolyLane` enum is DUPLICATED in two files.
- `SequencerEngine.hpp:172,173,226` — `laneSign_/laneSignPending_/macroLaneSign_ = {1,1,1,1,1,1}` are
  hardcoded 6-element initializers that do NOT auto-scale with `NUM_STRANDS`.

## Count constants — bump together (these gate everything)
Adding the q-mix strand/lane means updating each, IN LOCKSTEP:
- `NUM_STRANDS` 6→7 (dotModular / LaneMapping). Most engine arrays are sized `[NUM_STRANDS]` and loops are
  `for (s < NUM_STRANDS)` — these AUTO-SCALE. Good. Verify no `< 6` literal stands in for it.
- `PL_LANES` 4→5 and add `PL_QMIX` — **in BOTH** `SequencerEngine.hpp:298` AND `PatternEngine.hpp:108`.
- `SandsGrid.hpp`: `MONO_LANES`/`EAST_LANES` 6→7, `POLY_LANES` 4→5 (in lockstep with the engine strand).
- `LaneMapping.hpp`: editor order (QMIX at 2) + the mapping tables (already staged by CC).

## The traps — hardcoded things that do NOT auto-scale (grep + fix each)
- Initializer lists sized to the old count: `laneSign_ = {1,1,1,1,1,1}` and siblings → add the 7th element
  (or initialise by loop). `SequencerEngine.hpp:172,173,226`.
- Hardcoded lane bounds/ranges: any `l < 3`, `l < 4`, `< 6`, or "spread lanes 0..3" (`SequencerEngine.hpp:374`)
  that stands in for PL_LANES/NUM_STRANDS. Grep: `grep -rnE "< *[3-7] *;|0\.\.3|1,1,1,1,1,1" src/dsp`.
- Any per-lane switch/if-chain that lists lanes explicitly (e.g. `polyLaneToStrand`, `PL_ACCENT ? …`) —
  add the `PL_QMIX` case. `SequencerEngine.hpp:482`.

## Per-subsystem parity (do what MELODY/OCTAVE do)
- **Engine generation** (`executeStep`, `precomputeDraws`): draw + LOR-shape q-mix like melody; per-voice via
  cursor-packing (see PHILOX_NONCE_ADDRESSING.md). Seed/reseed already mirrored on `feat/qmix-rng-stream`.
- **ID enums** (`Monsoon.hpp`): add q-mix's LEN/OFF/ROT param, CV input, atten, spread trim, direction,
  prob-out, and poly-param block, as a per-lane BLOCK parallel to the others — NOT a `// New:` wedge.
  Pre-release (breaking is free): consider regularising the accent wedges + the split octave group at the
  same time so the whole enum is uniform.
- **Persistence** (`MonsoonPersistenceManager`): add q-mix's `sourceSelectSeedFloat` / `…SeedPending` /
  `…ReseedRoll*` to save+load, mirroring `rhythm*`/`melody*` (else q-mix state won't survive a patch save).
- **LaneMapping**: as above (CC staging).
- **UI editor** (`SandsVisualEditorV4`): q-mix draws as a normal lane at slot 2 (label "QMIX"); no gap.
- **Panel generators** (`gen_macro_mono.py`, `gen_east_clean.py`): full control lane + (Macro) a 5th MIX-IN
  group — done on `feat/sands-qmix-geometry`.
- **Poly / expanders** (`Straits`, `Causeway`, `Lantern`, `Keppel`, `Intertropical`, `ChangiT2/T3`,
  `OwnerCell`): wherever lanes are enumerated for poly voice control / display, include q-mix. These are the
  files accent touches (grep `accent` for the full set) — q-mix should touch the melody-side equivalents.
- **Big5 → Big6**: q-mix is a headline modulatable on Monsoon (CONTEXT_RECOVERY) — add it to that set.

## Recommended sequencing
1. Bump the count constants + `PL_QMIX` (both enum copies) + fix the hardcoded initializers/bounds first —
   that's the accent-bug class, and it's mechanical.
2. Then the ID block, persistence, generation consumer, UI, expanders — each mirroring melody/octave.
3. Verify: every `accent`/`ACCENT` site has a q-mix sibling (unless genuinely rhythm-side-only), and the
   whole unit suite stays green (test/run_all.sh).

## Added scope: per-voice knobs, dice controls, panels, modulators (all follow existing patterns)
q-mix's controls, dice, panels and modulation routing mirror what rhythm/melody already have — verified
in code, so this is pattern-application, not new design:

- **Straits — per-voice q-mix knobs.** Pattern: `POLY_<LANE>_PARAM_1..15` in `Monsoon.hpp` bound by a loop
  in `MonsoonStraitsExpander.hpp:87` (`configParam(POLY_REST_PARAM_1 + i, …)`). Add `POLY_QMIX_PARAM_1..15`
  + the loop. NOTE: the existing pattern is **15** poly params (voice 0 is Monsoon's own knob), = 16 voices
  with Monsoon's. "16 on Straits" would diverge — confirm 15 (pattern-consistent) vs a literal 16.
- **Monsoon — 1 mono q-mix knob + the dice family.** The knob mirrors the mono lane knobs (e.g.
  `ACCENT_KNOB`). The dice is NOT one button: rhythm/melody each have `DICE_{R,M}_PARAM`,
  `DICE_SLEW_{R,M}_PARAM`, `DICE_TRIAL_{R,M}_PARAM`, `LAST_DICE_{R,M}_PARAM` (`Monsoon.hpp:98,177,190,195`).
  q-mix needs the full parallel `*_Q` set (dice + trial/B→A + slew + last-dice), consistent with the
  per-stream dice model (one dice each for rhythm/melody/CA/q-mix).
- **Wider panels.** Both Monsoon and Straits panels grow to fit the new knob(s)/dice — same generator
  geometry work as the Sands q-mix lane: `gen_straits.py` (HP 26 → wider), Monsoon via
  `embed_monsoon.py`/`gen_controls.py`. Widen HP + relay out; keep the shared design tokens.
- **Raffles — gate-trigger q-mix redice.** Pattern exists per stream: `RAFFLES_GATE_REDICE_{R,M}`,
  `RAFFLES_GATE_LASTDICE_{R,M}` (`Monsoon.hpp:350,359`). Add `RAFFLES_GATE_REDICE_Q` /
  `RAFFLES_GATE_LASTDICE_Q` so Raffles can fire q-mix dice like the others.
- **Junction + Causeway — q-mix as a modulation TARGET.** These modulators route to Monsoon/Straits
  params; add the q-mix knob + dice ids to their target lists/routing so they can modulate q-mix's
  probability and re-dice it, exactly as they do for the other lanes.

All of the above are covered by the count-constant + no-wedge + persistence rules above: add the ids as
uniform per-lane blocks, update any hardcoded per-lane/per-voice bounds, and persist any new state.
