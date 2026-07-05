# Straits East/West redesign — concept note (FOR LATER)

Status: **idea capture, not a spec, not scheduled.** Recording the direction so it isn't
lost; detailed design happens later. Cross-refs: `SANDS_TOPOLOGY_RESOLVER_PLAN.md`,
`SANDS_LANE_INDEX_AUDIT.md`, `CODEBASE_REFACTOR_REVIEW.md`.

> SCOPE CORRECTION: this is primarily about the **base East/West POLY EXPANDERS** —
> the modules (`MonsoonStraitsEastExpander` / `MonsoonStraitWestExpander`) that carry the
> actual poly output jacks and base level knobs and are **required before any polyphony
> works**. It is NOT primarily about the visual editor. The editor cleanup is a separate,
> lesser concern.

---

## Current state (the "before")

- **`MonsoonStraitsEastExpander`** (live): exposes **7 voices** of individual outputs —
  `POLY_GATE_OUT_1..7`, `POLY_CV_OUT_1..7`, `POLY_ACCENT_OUT_1..7` (21 mono jacks).
  Covers Monsoon voices 2–8.
- **`MonsoonStraitWestExpander`** (live): extends with 8 more voices, Monsoon voices 9–16.
- Voice cap is therefore **7 (East alone) or 15 (East+West)** —
  `polyOutCap = straitWest ? 15 : 7` (`MonsoonExpanderManager.cpp:29`).
- CV-modulation inputs currently live ON the East/West surfaces (mixed in with base).
- Mono (voice 1) is wedged in as "slot 0", sharing addressing with poly — the documented
  off-by-one breeding ground.

Problems: the 8-vs-16 East/West split is an artificial boundary; outputs are 21 individual
jacks instead of poly cables; CV ins clutter the base modules; mono and poly share
storage/addressing.

---

## The redesign (user's six points)

1. **Primary subject:** the base East/West poly expanders (pre-polyphony requirement), not
   the editor.

2. **Drop the 8-vs-16 distinction.** No more East=2–8 / West=9–16 artificial split. One
   coherent voice model (the East/West *base* boundary goes away; whether West survives as
   anything is an open question below).

3. **Straits East base = REST + ACCENT level knobs, laid out as a 4-column × 8-row grid.**
   4 columns of level knobs: **2 columns REST + 2 columns ACCENT**, **8 rows** → 16 REST +
   16 ACCENT = 32 knobs. Uses **slightly-smaller versions of the equivalent Monsoon knobs**
   for coherent design, and **fits in sensible HP** (this is the HP-feasibility resolution
   — see West question below). The knobs mirror Monsoon's own REST/ACCENT controls; the
   base *levels* live here.
   - DETAIL TO CONFIRM: 4×8 = 16 per group, not the 15 poly voices stated earlier. Either
     all **16 voices get a real base knob** (voice 1/mono included — consistent with point
     4's "voice 1 duplicate" if mono also has its own level here), or it's 15 voices + 1
     spare per group. The clean reading is 16 (mono voice 1 gets a knob too). Decide which.

4. **Straits East exposes POLY outputs (poly cables, not 21 jacks):** at least poly
   **gate**, poly **accent gate**, and poly **CV** outs. **16-voice** poly cables, with
   **voice 1 a duplicate** (the mono voice copied into channel 1 of the poly cable, so a
   single poly cable carries mono@1 + the 15 poly voices). Replaces the current 7×
   individual `_OUT_1..7` jacks with 16-channel polyphonic outputs.

5. **CV modulation → its own CV expander.** The CV-mod inputs currently on East/West move
   to a dedicated CV expander, **with a clean rethink of poly vs mono CV INs** (how a CV
   input maps to one voice vs all voices vs the mono voice — currently muddy).

6. **Mono jack + mono CV outs → another (separate) expander.** The mono signal path gets
   its own module with its own mono output jack(s) + mono CV outs, so mono stops being
   "channel 1 of a poly thing" on the output side too.

### Resulting module shape (sketch)

- **Straits East (base poly):** REST + ACCENT base level knobs in a **4-col × 8-row grid**
  (2 cols REST + 2 cols ACCENT = 32 knobs, slightly-smaller Monsoon-style, sensible HP);
  poly gate / poly accent-gate / poly CV outs as 16ch cables (ch1 = mono duplicate). The
  "levels + poly outs" module.
- **CV expander:** poly + mono CV modulation INPUTS, rethought.
- **Mono expander:** mono output jack(s) + mono CV outs.
- **West:** likely **retired** — the 4×8 knob grid fits one East module in sensible HP, so
  there's no voice-range split and no overflow need (was a ghost on the visual side anyway,
  CODEBASE_REFACTOR_REVIEW B0). Returns only if out-jacks alone push past sensible HP.

---

## Why poly vs mono separation matters here (ties to topology)

The off-by-ones all session came from mono (voice 1) and poly sharing one
storage/addressing scheme with "slot 0 is special" special-casing. This redesign makes the
separation **physical**:

- mono outs on their own module (point 6),
- poly outs as a 16ch cable with mono merely *duplicated* into ch1 (point 4) — a copy, not
  a shared slot,
- CV ins explicitly split poly vs mono (point 5).

That mirrors the `SandsTopology` resolver's clean `voice 0 = mono` vs poly separation in
the decision layer. The module boundaries should follow the resolver's seams, which is why
this is sequenced AFTER the topology work.

---

## Open questions to settle before designing (not now)

1. **West's fate:** the HP worry is largely resolved — a 4-col × 8-row grid of slightly-
   smaller Monsoon-style knobs (32 knobs) plus the poly-cable out jacks fits sensible HP on
   **one** East module. So West likely **retires** as a base module (no voice-range split,
   no overflow need). Confirm the full faceplate (32 knobs + poly gate/accent/CV out jacks
   + any in) still fits comfortably; if jacks push it over, West could return purely as an
   output-jack overflow, but that's now the exception, not the plan.
2. **"Voice 1 duplicate" semantics:** is ch1 of the poly cable always == the mono voice, or
   only when mono is active? Define what ch1 carries when the patch is poly-only.
3. **CV expander mapping:** how does a CV input address voices? Per-voice poly CV in (16ch
   cable) vs one CV → all voices vs a mono CV lane. This is the "rethink poly vs mono CV
   ins" — needs an explicit addressing model (the same discipline as the lane-index audit).
4. **Mono expander vs parent Monsoon:** the parent already has a mono path. Does the mono
   expander duplicate/extend it, or does mono stay on the parent and only *poly* gets
   expanders? Decide the mono ownership boundary.
5. **Ownership/levels location:** REST/ACCENT *base levels* now live on East (point 3); the
   ownership cells (who owns each lane) currently live on the visual editor. Confirm the
   base-level knobs and the ownership flags stay coherent across the base module + editor +
   the topology resolver (the resolver is the seam both consult).
6. **Backward compat:** new poly-cable outputs + new module split changes the param/IO map.
   Existing patches break unless migrated. Clean break ("v2" module IDs) vs migration path.
7. **Voice count param:** with one base module, where does "how many poly voices" live, and
   how does it interact with the poly-cable channel count (always 16 vs N)?

---

## Sequencing

Post-topology project. The module boundaries should follow the seams the `SandsTopology`
resolver exposes (clean mono/poly + ownership separation), so: finish topology (steps 3–6)
→ then design this against the resolver as the contract. No action now.
