# QMIX OCTAVE — build plan (a second Qmix read on Straits: degree vs octave as two axes), staged

Spec: `docs/design/CC_PROMPT_QMIX_OCTAVE.md` (Design — Rodney, decided). This plan carries the
implementation map and the adjudications made while landing it. Branch: `feat/qmix-octave`, cut
from `feat/phase-tier-subphase` — the 32-step chain's current tip (`feat/steps-32-stage1` is an
ancestor), per Rodney: "Another task that can also branch off the 32 step branch". The spec's
own siting line ("Build on lane-expander-refactor") predates the 32-step work;
`lane-expander-refactor` is an ancestor of `feat/steps-32-stage1`, so building on the chain tip
contains it in full.

## THE TREE THE SPEC LANDS ON (deltas vs the spec's WHAT EXISTS)

The CC spec was written against lane-expander-refactor. Since then the 32-step chain, the
phase-duals/gaps build and the phase-tier build landed on this line; the grounding section
reads slightly differently here:

- `genPitchLive` sits at PatternEngine.cpp:292 (the spec says :282 — the 32-step ring comments
  shifted it). The substance is unchanged: it takes SEPARATE randoms — `r_semi` (the degree,
  via `pickSemitone`) and `r_oct` (the octave roll) — the two axes are already split at
  GENERATION, exactly as the spec says.
- The Qmix Philox stream, the per-voice level, the CA scatters, the polarity ruling (§29:
  0 = generated, 1 = quantised) — all as the spec grounds them.

## THE CROSSFADE SITE (the spec's IMPLEMENTATION NOTE — traced, not assumed)

The mux is **`SequencerEngine::voicePitch`** (SequencerEngine.hpp:1166):

```cpp
float voicePitch(VoiceSlot voiceIdx, int& outSem, const PatternInput& input,
                 float r_semi, float r_oct, bool forceGenerated = false) {
    if (quantiserPitchSource && !forceGenerated) {
        const int srcVi = pe.caInputCvSrcRow(vi);          // CA melody plane (QMIX_LANE_PARITY)
        float v = quantize(quantiserCV[rvi]);              // THE EXTERNAL operand: the WHOLE pitch
        outSem = <the quantised degree, for the LEDs>;
        return v;
    }
    return pe.genPitchLive(outSem, input, r_semi, r_oct);  // THE GENERATED operand: the WHOLE pitch
}
```

The DECISION is made by the callers, against the per-voice level `voices[·].qmixLevel`:

- Mono (`executeStep`, SequencerEngine.cpp:1256):
  `qmixUseGenerated = quantiserPitchSource && (r_qmix >= voices[monoSlot].qmixLevel)`
- Poly (`executePolyVoice`, SequencerEngine.cpp:2123):
  `qmixUseGenerated = quantiserPitchSource && (r_qmix_voice >= v.qmixLevel)`
  (r_qmix drawn per voice from `warpRow(v, PL_QMIX)`, already CA-scattered.)

Polarity (MODE_COLLAPSE_6_TO_3 §29): the draw crosses UPWARD — `r >= level` → generated;
level 0 = always generated, 1 = always quantised. The level's sole writer is ModeController
(poly: MonsoonModeController.cpp:56 from `getEffectivePolyQmix` — the Straits per-voice Q-MIX
knob + the Causeway QMIX_CV × att, clamped, patched-detection; mono: :155 from
`getEffectiveMonoQmix(QMIX_LEVEL_PARAM)`).

Because the mux switches the WHOLE pitch voltage, ONE Qmix value governs BOTH axes today: the
degree (which `pickSemitone(r_semi)` names) and the octave (which the `r_oct` roll lands) are
generated or external TOGETHER. That is the single crossfade the spec splits.

## THE MODEL (the subgate pattern: multiple reads of one stream, different thresholds)

Two independent reads of the SAME per-note Qmix random `r_qmix`, each against its own
threshold:

- **QMIX DEGREE** — the existing threshold (`qmixLevel`): `r_qmix >= qmixLevel` → the
  within-octave degree is GENERATED (`pickSemitone(r_semi)`), else external (the quantised CV's
  within-octave fraction).
- **QMIX OCTAVE** — the new threshold: `r_qmix >= qmixOctLevel` → the octave is GENERATED (the
  `r_oct` roll into [octaveLo, octaveHi]), else external (the quantised CV's whole-volt
  position).

The four cases per note, per voice:

| degree | octave | pitch |
|---|---|---|
| gen | gen | `genPitchLive(...)` verbatim (today's generated branch) |
| ext | ext | `quantize(quantiserCV[rvi])` verbatim (today's external branch) |
| gen | ext | external OCTAVE floor + generated DEGREE term |
| ext | gen | generated OCTAVE term + external DEGREE fraction |

The mixed rows are the feature; the equal rows must stay bit-identical (the gate).

## THE COMPOSITION (adjudication 2)

`genPitchLive` writes the generated pitch with a per-formula grouping; the split honours each
formula's own grouping rather than inventing a new one:

- 12-TET: `v = (oct - 4) + (sem + transpose)/12` → OCT term `oct-4`, DEGREE term
  `(sem + transpose)/12` — the 12-TET transpose rides the DEGREE term (the formula's own
  placement).
- Custom tuning (Micro): `v = (oct - 4) + degreeVolts(sem) + octShift` → OCT term
  `oct - 4 + octShift`, DEGREE term `degreeVolts(sem)` — transpose is OCTAVES-ONLY (Rodney's
  Phase-1 ruling), so it rides the OCTAVE term.

Mixed rows take OCT from one source and DEGREE from the other:

- gen degree, ext octave: `v = pe_clamp(floor(vExt) + DEG_GEN, -5, +5)`
- ext degree, gen octave: `v = pe_clamp(OCT_GEN + (vExt - floor(vExt)), -5, +5)` — **the
  spec's musical case**: the external theme's degree rides the voice's OWN generated octave
  (bass pinned low, lead high, pad mid, all tracking the one melody).

The external operand in the mixed rows uses the SAME CA-routed voltage the both-external row
uses (`caInputCvSrcRow` — the QMIX_LANE_PARITY blend law: the input-CV operand rides CA's
melody plane). Mixed rows are NEW behaviour — they clamp to [-5,+5] like the generated path
(the external path's raw return is untouched in its own row). `outSem` names the DEGREE the
note plays, from the DEGREE source (gen → the picked semitone; ext → the quantised fraction,
the same expression the both-external row uses) — the markSemi correctness law ("what we
picked" and "what CA/Lantern see" never drift).

`genPitchLive` is re-expressed on a new `genPitchSplit` helper that returns the components
(`sem`, `octTerm`, `degTerm`, `shiftTerm`) with EXACT float-op association preserved:
12-TET `v = octTerm + degTerm` (+ 0 shift — exact identity, no -0 in range); Micro
`v = (octTerm + degTerm) + shiftTerm` — the same left-to-right order as today. The both-
generated row calls the same code path, so bit-identity is by construction.

## THE ENGINE-SIDE DEFAULT (adjudication 3 — the subResLive precedent)

`PolyVoice` grows `float qmixOctLevel` + `bool qmixOctLive = false`, and one accessor
`qmixOctResolved()` = `qmixOctLive ? qmixOctLevel : qmixLevel`. The engine's octave decision
compares `r_qmix >= qmixOctResolved()`. A rack-free engine that has never been through
ModeController's per-block write (every existing test constructs engines directly, and several
pin marches with hand-set `qmixLevel` values) keeps `qmixOctLive == false` → the octave
threshold IS the degree threshold → the two decisions are the same comparison on the same
draw → bit-identical, no re-pins. This is the exact shape of the `subRes`/`subResLive` gate
("default false ⟹ byte-identity for any engine nobody resolved"). ModeController is the sole
writer that flips `qmixOctLive` (with the bank in place, Stage 2).

## STAGE 1 — THE SPLIT, NO NEW CONTROL (bit-identical; the split itself is the proof)

- `PatternEngine::genPitchSplit` (new, PatternEngine.hpp/.cpp) — the components helper;
  `genPitchLive` re-expressed on it (association-exact, above). No behavioural change.
- `PolyVoice`: `qmixOctLevel` (0.0) + `qmixOctLive` (false) + `qmixOctResolved()`.
- `voicePitch` gains the octave decision `octForceGenerated` (NO default — both callers pass
  it, so a future caller cannot silently get one axis's default wrong):
  `voicePitch(voiceIdx, outSem, input, r_semi, r_oct, forceGenerated, octForceGenerated)`.
  Truth table inside: both-external = today's external branch verbatim; both-generated =
  today's generated branch verbatim (`genPitchLive`); mixed = the composition above.
- Callers: mono :1256 and poly :2123 compute BOTH decisions from the same `r_qmix`:
  `qmixOctUseGenerated = quantiserPitchSource && (r_qmix >= v.qmixOctResolved())`.
- `result.qmixHit` stays the DEGREE decision (adjudication 6): its consumers (the q-mix source
  latch, the Lantern pairings) mean "this note used the generated pitch" — the degree-axis
  reading. The octave axis gets no new LED in this build.
- Tests: `test_qmix_octave.cpp` (registered in run_all.sh) — the 4-case truth table at the
  voltage level with hand-computed expected values; the BIT-IDENTITY gate: full marches through
  quantiser modes with varied `qmixLevel` (0 / mid / 1, several voices, both polarity
  extremes), digests bit-identical against the pre-split engine for both rack-free engines
  (nobody resolved) and controller-resolved engines with nothing authored; outSem naming;
  clamp; CA-routed external operand in the mixed rows.

## STAGE 2 — THE BANK + THE GLOBAL KNOB (bit-identical until moved)

- `src/dsp/QmixOctave.hpp` (NEW, rack-free — the SubgateKnobs discipline):
  `struct Bank { float globalTh = 0.5f; bool followDegree = true; float ownTh[16];
  bool ownActive[16]; }` + `reset()` + `serialize/parse` (the SubgateBank codec shape:
  authored cells only, missing keys ⟹ defaults, short/wrong-typed arrays keep the default
  cell) + `inline float resolve(bool ownActive, float ownTh, bool followDegree, float globalTh,
  float degreeValue)` = `clamp01(ownActive ? ownTh : (followDegree ? degreeValue : globalTh))`.
  **No delta planes** (adjudication 4/5): the spec asks for a knob; the subgate per-voice
  panels carry no mod jacks post-merge (modulation is Causeway and not in scope); no deltas ⟹
  no audio-thread publisher ⟹ no watchdog/heartbeat machinery for this bank.
- `Monsoon::EditorState` gains `dotModular::QmixOctave::Bank qmixOctave;` + the accessor block
  (the subgate accessors' shape): `get/setQmixOctGlobal`, `is/setQmixOctFollowDegree`,
  `isQmixOctOwnActive`, `setQmixOctOwn` (touch = claim), `clearQmixOctOwnAll`, the resolver
  `qmixOctaveThreshold(storageSlot)` (own > { follow-degree | global }; the follow tier reads
  the voice's `qmixLevel` — the subgate resolver's order-dependency note applies verbatim:
  called AFTER the qmixLevel write in the same controller pass), and the display resolution
  `qmixOctaveDisplayThreshold(storageSlot)` (the one authority the knobs show).
- Persistence (MonsoonPersistenceManager, additive keys, the `editorSubgate` precedent):
  `QmixOctave::Bank::serialize(m->editor.qmixOctave, root, "editorQmixOct")` /
  `parse(root, "editorQmixOct", ...)`. Absent keys ⟹ follow-degree defaults ⟹ old patches
  bit-identical.
- ModeController: poly — inside `updatePolyVoiceRest_`'s loop, right after the
  `qmixLevel` write (the resolver's follow tier reads it): `voices[vs].qmixOctLevel =
  mainModule->qmixOctaveThreshold(i); voices[vs].qmixOctLive = true;`. Mono — in
  `updatePatternInput`, after the mono `qmixLevel` write: `qmixOctaveThreshold(0)`. Same
  BigFive latch class (the early-return above gates the whole loop). Without a mainModule the
  fields are never written → `qmixOctLive` stays false → follows degree (Monsoon alone:
  exactly today).
- The GLOBAL knob on the Straits base (the de-param law — store-backed, no Rack param):
  `gen_straits_base.py` grows ONE anchor `param_qoct_g` in the freed half of the VOICES band
  (left of the voice-count knob, the base grid is otherwise full — adjudication 7), the qmix
  violet tint, seat ring like the subgate globals; the widget binds
  `redDot::bindStoreKnob<Monsoon, Tag_Grey_Trim_Bar>` → `get/setQmixOctGlobal`, caption
  "Q-MIX OCT (global)" drawn widget-side (the Cantonment idiom — the SVG carries no text).
  Knob default 0.5, inert under the follow-degree default.
- The source switch lives in the base's context menu (the subgate per-decision switch idiom —
  the switches are menu check items, not panel controls): under a new "Q-MIX OCT source (when
  no own subpanel)" label: "follow degree q-mix" (default) / "global knob" — two check items
  reading/writing `is/setQmixOctFollowDegree`. Rodney's option list, verbatim mapping:
  "follow main qmix poly voice subpanel for both pitch and octave" = follow-degree;
  "global knob on straits for octave" = the global tier; "own sub panel" = Stage 3.
- Tests: the bank unit battery (test_qmix_octave grows the section) — the resolve truth
  table, bank defaults, persistence round-trip, absent keys, short/wrong-typed arrays (the
  test_subgate_knobs battery mirrored); the controller resolution pins (nothing authored ⟹
  resolved == the voice's qmixLevel bit-exact, rack-free + controller-resolved both).

## STAGE 3 — THE PER-VOICE SUBPANEL (the subgate-knob subpanel build, reused)

- ONE new attachable panel: `StraitsQmixOctPanelModule` (+ Widget) — the
  StraitsSubgatePanelModule/Widget build verbatim: `config(0,0,0,0)` pure attach surface, 16
  StoreKnobs (touch = claim — tier 1), unclaimed cells DISPLAY the resolved three-way value
  (`qmixOctaveDisplayThreshold`), first drag starts from the shown value, double-click claims
  0.5, V1 row editable + never dims, v1..15 dim/lock beyond the active poly count (the lane
  law), the poly-dim source `activeVoices_` (the lane law), `needsResnap` + the left-link
  check (the StraitsLane hook, family-parameterised).
- Panel SVG: `gen_straits_lane.py` grows tint key `"qoct"` (the qmix violet family — ONE
  panel, so the family hue itself, no lightness stepping needed) + emits
  `StraitsQmixOctPanel_{dark,light}.svg` with `subgate=True` (the tint-neutral `param_sgv_`
  anchors, the left-gutter dot, no mod jack). Widget lettering: "Q-MIX" (tinted) over
  "OCTAVE" (ink) — the Cantonment idiom.
- Registration: `plugin.cpp` declares + registers `modelStraitsQmixOctPanel`, `hidden = true`
  (base-spawned only — the StraitsLane law).
- The base's context menu: a "Q-MIX OCT per-voice panel" check item under the subgate panel
  list; spawn/adopt/restore through the base's ordered/pending lists
  (`orderedQmixOctIds_` / `pendingQmixOct_`, json key `"qmixOctPanels"` — the `subgatePanels`
  shape verbatim), docked AFTER the subgate panels (dock order: lanes → subgate panels →
  qmix-oct panel); `isStraitsSubgateChainModel` grows the QOCT panel model (the seam-filler
  family + the resnap chain); "Clear per-voice overrides" clears BOTH banks' own cells (the
  menu item's semantic: all per-voice claims on this base's family).
- The most-flexible end state (Rodney: "per voice separate qmix probability for pitch within
  octave and octave"): a voice with an own QOCT claim AND a per-voice Q-MIX level (the existing
  Straits lane knob) thresholds the two axes INDEPENDENTLY on the same draw — the four-case
  table fully reachable per voice.
- Tests: the spawn/adopt/restore structural pins (the subgate panel wheel's shape), the
  display-resolution pins (own claim wins / follow-degree / global), the anchor/bind audit
  counts move, run_all green both legs.

## GATE (the spec's, every stage)

- ≤16 goldens bit-identical at every stage with the octave threshold following degree;
  GOLDEN_A untouched; no re-pins.
- New pins: octave threshold at the generated extreme → octave always generated; at the
  external extreme → octave always from CV; mid → the mixed composition reachable.
  The musical-case loadout: shared external degree + per-voice-divergent generated octave.
- run_all green (both SDK legs), warn sweep clean, anchor/bind audit green.

## ADJUDICATIONS (decisions made while landing)

1. **The crossfade site** (the spec's IMPLEMENTATION NOTE): `voicePitch` is the mux; the
   decisions live at the two call sites (:1256 mono, :2123 poly) against `voices[·].qmixLevel`;
   polarity §29 unchanged. The split adds a SECOND comparison on the SAME `r_qmix` draw at
   those sites — no new random, no new stream (the subgate pattern: multiple reads of one
   stream).
2. **The composition grouping**: each formula's own grouping (12-TET transpose → DEGREE term;
   Micro octaves-only transpose → OCTAVE term); `outSem` follows the DEGREE source; mixed rows
   clamp [-5,+5]; the external operand is CA-routed in every external row. The both-equal rows
   reuse today's code paths verbatim — bit-identity by construction, not by pin alone.
3. **The engine default is the FLAG, not a mirrored field**: `qmixOctLive=false` keeps any
   rack-free engine (all existing tests, hand-set `qmixLevel` marches included) bit-identical
   — a mirrored float default (0.0) would create silent MIXED cases in every test that sets
   `qmixLevel != 0` and drives a quantiser mode. The subResLive precedent, same reasoning.
4. **No mod jack for the octave knob**: the spec asks for "a Qmix-octave knob"; the subgate
   per-voice panels ship none (modulation is Causeway, CC_PROMPT_SUBGATE_MERGE_ONTO_STRAITS_
   BASE), and no mod path is named. If Rodney wants one later it lands as the delta-plane
   machinery's fourth use — additive, no rework of this build.
5. **Bank placement**: `Monsoon::EditorState` (the engine host both families attach to), the
   de-param law (store-backed knobs, UI-thread writers), no Rack params, no delta planes, no
   watchdog (nothing publishes audio-side). The subgate bank's shape minus the axes this
   decision does not have (one threshold per voice, no level/dec grid).
6. **`qmixHit` stays the DEGREE decision** — its consumers read "used the generated pitch"
   for the pitch source latch / Lantern pairing; the octave axis adds no LED. The fader-flash
   `isQuant` likewise stays the degree-axis reading.
7. **Panel placement**: the global knob sits in the VOICES band's freed left half (the base's
   2×4 subgate grid + IO row are full; a third row band would push the IO row or the wave
   footer). Same band, same seat-ring idiom, qmix tint, widget caption — "similar to other
   items on main straits panel" (Rodney). The per-voice panel is the lane-panel artwork
   (6/6/4, 11HP, seam rails) with the qoct tint — the subgate panels' exact build.
8. **The stage order** follows the spec verbatim: the split with no control (Stage 1) proves
   bit-identity BEFORE any surface exists; the global tier lands before the own tier (the
   three-way's tier-2 before tier-1), so the default path is pinned at each stage.
