# Shophouse — scale expander (design note)

Status: **design capture. Not scheduled to build yet** — but the topology work it waited
behind is essentially done, so the design can firm up. Shophouse extracts + extends the
scale behaviour that currently lives in Monsoon's context menu, the same way the visual
editors began as context-menu explorations.

---

## Current scale implementation (baseline — it's basic)

Lives in `MonsoonScaleManager` (`src/dsp/managers/MonsoonScaleManager.{hpp,cpp}`), driven by
Monsoon's **context menu**:
- Context menu sets **scale** (`lastSelectedScale`) and **root note** (`scaleRoot`).
- `activeScaleMask` = 12-bit mask of in-scale semitones (`calculateMask(root, scale)`).
- **Without scale lock:** the mask is only a **visual guide** — in-scale faders indicated,
  but all 12 semitone faders (`SEMI0_PARAM..SEMI11`) are read normally.
- **With scale lock (`lockScaleNotes`):** DESTRUCTIVE. `updateScaleMask()`:
  - forces out-of-scale faders to 0 (`setValue(0)`) AND pins their range (`min=max=0`),
  - `redistributeWeights()` — the "nudge": redistributes out-of-scale weight to nearest
    in-scale neighbours on scale change, MUTATING in-scale fader values too.
- Engine read path `getSemitoneWeight(sem)` ALREADY returns 0 for out-of-scale-when-locked
  (`if (lockScaleNotes && !(mask & bit)) return 0`) — read at Monsoon.cpp:128 and :236 for
  pitch selection.

Key point: the engine ALREADY enforces scale as zero-probability at READ time. The
destructiveness is the SEPARATE physical fader forcing + redistribution in updateScaleMask.

---

## Two ORTHOGONAL things (this was conflated in an earlier draft — corrected)

**(A) Lock mode = guide vs enforce (a real user choice — KEEP it).** This is the semantic
switch, NOT a destructive/non-destructive thing:
- **guide** (lock off): the scale mask only INDICATES what's in-scale; out-of-scale notes
  still play at their fader probability. A visual hint.
- **enforce** (lock on): out-of-scale notes are silenced — read as zero probability. The
  scale is strictly enforced.
On **Shophouse this mode is called "Conservation."** Conservation ON = enforce the scale
(only in-scale notes sound); OFF = scale is a visual guide, everything can still play. (Nice
double meaning: it conserves the SCALE — enforcement — and conserves the FADER VALUES —
non-destructive, see B.)

**(B) HOW enforcement is done = destructive vs non-destructive (an implementation change).**
Independent of A. Today enforcement (when lock is on) is DESTRUCTIVE: `updateScaleMask` forces
out-of-scale faders to 0 + pins range + `redistributeWeights`. Change it to NON-DESTRUCTIVE:
- **Keep** the read-time zero-enforcement in `getSemitoneWeight` (already correct — that IS
  the enforcement, and it's already non-destructive at the read).
- **Remove** the physical forcing in `updateScaleMask`: no `setValue(0)`, no `min=max=0`, no
  `redistributeWeights`. Faders keep their user values.
- **Dim** out-of-scale faders instead of pinning them.
- Net: Conservation still enforces the scale (out-of-scale read as zero), but does so WITHOUT
  destroying the underlying fader values — so toggling conservation or changing scale reveals
  the faders unchanged. Same principle as the non-destructive spread work (value persists; a
  mask gates the read). The guide-vs-enforce CHOICE (Conservation on/off) remains.

---

## Scale list + phrase-boundary modulation (the new capability)

- **A small list of scales** (≈4 entries feels right). Each entry = a (scale, root) pair —
  same two choices the context menu offers today. The user edits the list ON Shophouse.
- **Modulate through the list.** New scale applies **only at the phrase boundary** (never
  mid-phrase). This aligns with the meloDICER **phrase-boundary reset** (see
  LEGATO_TIE_REDESIGN.md) — gates already stop/reset at the boundary, so a scale switch at
  that same clean cut is coherent (no notes mid-flight to disrupt).
- **How the list position is driven — OPEN, two candidates:**
  - (a) **Gate/trigger, dice-style:** forward/back trigger inputs step the list position;
    like the dice, the change is QUEUED and applied at the next phrase boundary. Bidirectional
    (fwd/back gates) fits the fwd/reverse phase theme.
  - (b) **CV sampled at the boundary:** a CV input whose value is SAMPLED at each phrase
    boundary maps to a list index. Continuous control, still boundary-quantised.
  - Both share "decide freely, apply at boundary" (like slew/dice queuing). Could support
    both. Decide primary mechanism.

---

## How scale reaches the notes (stays in the existing read path)

"Quant to scale" here does NOT mean pitch quantisation — it means **zero-probability
enforcement for out-of-scale semitones in note-pitch SELECTION**. That already happens in
`getSemitoneWeight` (the mask gates the weight to 0). So Shophouse doesn't need a new engine
path — it feeds the active (scale, root) → mask into the existing ScaleManager, and the
engine keeps reading weights the same way. Shophouse is a control expander that WRITES the
active scale/root (unlike Lantern which only reads), but it writes into the existing
scale-state slot, not a new engine mechanism.

Boundary-applied: the list→active-scale switch is committed at the phrase boundary, so the
mask the engine reads changes only at that clean cut.

---

## Migration — Shophouse eventually REPLACES the context menu

Like the visual editors (which began as context-menu explorations, then became modules),
Shophouse is meant to **replace** Monsoon's scale/root context menu once it's working.
Path:
- Phase 1: Shophouse coexists with the context menu (menu still works; Shophouse overrides
  the active scale/root when attached). Non-destructive fader behaviour can land on the
  PARENT first (it's a ScaleManager change), independent of Shophouse.
- Phase 2: Shophouse adds the list + boundary modulation.
- Phase 3: retire the context-menu scale UI once Shophouse covers it (keep the underlying
  ScaleManager state + JSON for back-compat).

---

## Open questions

1. **Conservation (lock) mode STAYS** — it's the guide-vs-enforce choice, renamed
   "Conservation" on Shophouse. What changes is only the ENFORCEMENT MECHANISM: destructive
   fader-forcing → non-destructive read-time mask + dimming. (Corrected from an earlier draft
   that wrongly proposed dropping lock mode.)
2. **List modulation driver:** gate/dice-queued (a) vs CV-sampled-at-boundary (b) vs both.
3. **List length:** 4 confirmed, or configurable?
4. **List entry UI on Shophouse:** how does the user pick scale+root per slot — the same
   scale menu per slot, or faders/encoders? (Panel design question.)
5. **Interaction with the phrase-boundary reset:** confirm scale switch and gate-reset share
   the same boundary event cleanly (they should).
6. **Does Shophouse own root-note transposition too, or just scale-degree masking?** (Root is
   part of each list entry, so yes — but confirm root change is also boundary-quantised.)
7. **Multiple Monsoons / poly:** does the scale apply globally (all voices) as now? (Current
   mask is global — presumably unchanged.)

---

Design capture only. The non-destructive fader change is small and could land on the parent
ScaleManager independently; the list + boundary modulation is the new Shophouse module.
Post-topology, own build session.

---

## TODO (near release) — house-groove example patch

When Shophouse is built and release gets nearer, create an EXAMPLE PATCH: a house / deep-house
groove built around Shophouse, to ship as a demo / documentation / marketing asset.

Why it fits:
- House lives in a tight harmonic pocket — a few notes, a clear mode, everything locked to the
  groove. Conservation keeps Monsoon's probabilistic engine strictly in-scale so it never
  wanders off-key.
- The boundary-quantised scale LIST = mode/chord changes that land cleanly on the phrase
  boundary (e.g. minor pentatonic → Dorian vamp at the 16-bar mark), snapped to the loop, no
  fumble. That's a core house/deep-house move.
- The "Shophouse for house music" pun is a nice hook for the demo.

Framing: brings a TINY bit of what Scaler 3 does (scale/chord suggestion + mode changes) but
in a MODULAR, probabilistic, performance context — not a chord-suggestion plugin, but
scale-conservation + boundary-timed mode modulation feeding a stochastic sequencer. Position
it as "Scaler-flavoured harmonic control for a generative modular voice," not a Scaler clone.

Patch sketch (fill in when the module exists): Monsoon + Shophouse (Conservation on) driving a
house bassline/stab voice, a 4-on-the-floor clock, scale-list stepped fwd at phrase boundaries
for the arrangement, probability faders shaping the groove within the conserved scale.

---

## Panel concept — decisions (2026-07)

- **Literal shophouse design.** Lean into the actual building form (Peranakan shophouse:
  ornate pediment, shuttered upper-floor windows, ground-floor five-foot-way colonnade).
  Form-follows-meaning, in the dot.modular geography tradition (MBS, Straits Peranakan).
- **20HP** starting width (portrait ~101.6 × 128.5mm — suits a tall narrow shophouse facade).
- **Scale display: strong nice-to-have.** Concept A "the shutters" — the 12 semitones as a row
  of louvered window shutters: OPEN (teal-lit) = in-scale, CLOSED (dark) = out-of-scale. The
  facade literally shows the active scale. This is both the themed motif AND the functional
  scale display.
- **Scale list = the ground-floor shop units** (Concept C). ~4 arched units along the
  five-foot-way colonnade, each = a (scale, root) entry. Active unit lit; next-queued unit
  outlined. Combines A (upstairs = current scale's notes) + C (downstairs = the list).
- **Driver: CV, sampled at the phrase boundary.** One CV input → list index, read only at the
  boundary. (Leaning CV over fwd/back gates; could add gates later.) So jack layout = one CV
  IN (+ maybe a reset/sync), not a fwd/back pair.
- **Single scale = a 1-entry list.** KEY UNIFICATION: today's context-menu (scale, root) is
  just list slot 0. No "simple vs list mode" — it's always a list; current behaviour is the
  degenerate length-1 case. Migration = the existing scale/root becomes slot 0.
- **Conservation toggle** (guide vs enforce, per the corrected framing) — a switch + indicator
  at street level.

Layout sketch (portrait, top→bottom): pediment + wordmark / cornice / upper facade = 12
shutter-windows (scale display, 2 storeys × 6) / cornice / five-foot-way colonnade = 4 scale-
list arched units (active lit, queued outlined) / street level = CV IN jack + Conservation
toggle + list-position dots / screws at corners. Dark + light themes, Singapore-red accents.

---

## Non-destructive scale IS the display/store/engine (MVC) separation — same as spread/lock-mode

The destructive updateScaleMask is the SAME anti-pattern as the old spread bug: it collapses
STORE into a display/enforcement view. `setValue(0)` + `min=max=0` on an out-of-scale fader
clobbers the PARAM (the store — the user's weight, JSON-persisted) to represent an ENFORCEMENT
state. Identical mistake to forcing the spread trimpot; identical bug (toggle Conservation off or
change scale → user's fader values gone, just as spread didn't revert on reclaim).

Three-layer mapping (see DISPLAY_STORE_ENGINE_SEPARATION.md):
- STORE   = the 12 semitone fader params (SEMI0..11). User's weights. Persisted. NEVER written
            by enforcement.
- DISPLAY = fader render + DIM. Out-of-scale faders show dimmed (and may show 0-effect) while
            the stored value underneath is untouched. Derived from activeScaleMask.
- ENGINE  = getSemitoneWeight reads the store, gates to 0 via the mask when Conservation on.
            Read-time, non-destructive. ALREADY CORRECT.

The scale mask is the same kind of external gate as Macro ownership: it changes what's DISPLAYED
and what the ENGINE plays, without touching the STORE. So the fader wants the SAME treatment the
spread knob got (DimmableTrimpot's dimWhen/displayValueFn): dim on out-of-scale, leave the store
alone, optionally display the gated (0-effect) value while preserving the underlying weight.

Convergence worth noting: Monsoon context-menu "lock mode", Shophouse "Conservation", and the
spread cede/reclaim fix are ALL the same display/store/engine separation — the doc already
predicted lock mode would want this infrastructure. Build the fader-view layer mirroring
DimmableTrimpot rather than inventing a parallel mechanism.

Scope of THIS branch (feature/nondestructive-scale):
1. DATA/ALGORITHM (testable now): updateScaleMask stops writing the store — computes mask only;
   remove setValue(0)/min=max=0/redistributeWeights from the enforce path. Keep getSemitoneWeight
   read-time gate exactly. Keep the Conservation (lock) CHOICE.
2. VIEW/MVC (write, but needs in-Rack build to confirm render): semitone fader gains
   DimmableTrimpot-style dim-on-out-of-scale, reading the mask as a DISPLAY input, never writing
   the store. Flagged like the spread knob's displayValueFn.

---

## Fader-view layer — decisions (user) + the acceptance test

Decisions for out-of-scale faders under Conservation (enforce):
1. DIM, DON'T CHANGE POSITION (default): render dimmed at the TRUE stored position — user sees
   their real weight, greyed. Not dropped to zero visually (for now).
2. WRITE IT FLEXIBLE: the fader's rendered value goes through a swappable displayValueFn (the
   spread-knob pattern) so switching to "display zero position" later is a one-line change, not a
   rewrite. Flexibility = the MVC separation (display value distinct from store, pluggable).
3. ACCEPTANCE TEST (the real one): **the user must NEVER see an out-of-scale fader in its bright
   / active (lit-up) state** when Conservation is on. A bright fader falsely signals "this note
   is sounding" while enforcement silences it. Dimming is the visual truth of the read-time gate.
   Invariant the view layer must guarantee: out-of-scale + Conservation-on ⇒ always dimmed, never
   bright, ever.
4. SHOW MODULATION FOR NOW (even though Conservation forces the gated VALUE to zero): the fader
   may still show its modulation. NOTE the semantic tension — modulation moves a weight the engine
   reads as 0 (out-of-scale). Shown for now, but the mod-display path is a SEPARATE, clearly-
   marked, SWAPPABLE hook. STILL TODO: choose the final modulation-display method for faders
   (dim-in-place mod arc vs suppressed vs gated indicator). Keep localized so the final choice is
   a small change.

Structure (mirrors DimmableTrimpot's separated concerns) — three pluggable hooks on the fader:
  - dimWhen        → out-of-scale; drives the dim AND enforces the never-bright invariant.
  - displayValueFn → currently "stored position" (dim-in-place); trivially → "zero position".
  - mod-display    → currently "show modulation"; clearly marked swappable pending final method.
