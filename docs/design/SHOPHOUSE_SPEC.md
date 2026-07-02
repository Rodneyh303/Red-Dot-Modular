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
