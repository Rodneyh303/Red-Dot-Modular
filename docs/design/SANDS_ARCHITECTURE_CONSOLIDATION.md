# Sands Architecture — consolidation analysis (1 vs 2 vs 3 modules)

Status: **analysis / discussion, not actioned.** Captures the drift from the
original concept and options for pulling it back. Nothing here changes code yet.

## The original intent vs where we are

**Original concept — three tiers of granularity, each dead simple:**
- **Mono** — one voice, 6 lanes, direct editing. Simple.
- **East** — "broad strokes" poly. Set the poly voices' character.
- **Macro** — "detailed strokes" poly. Fine per-voice control.

**Where it drifted:** the East↔Macro relationship became a *mixer* with
sources, destinations, per-lane ownership transfer, per-voice send 2×2 grids,
and read-only display overwrites. The mental model is now "which module owns
this lane on this voice, and how much of the other one blends in" — which is
not what "broad vs detailed strokes" was supposed to mean.

The complexity lives almost entirely in one place: `combineLOR` /
`combineSpread` in `MonsoonExpanderManager.cpp`. Per lane, per voice, per item:

```
base  = ownerEast ? East's own LOR : (Macro present ? Macro base : East's LOR)
blend = (Macro present AND ownerEast) ? macroCVDelta * perVoiceSend : 0
result = clamp(base + blend)
```

Two things make this hard to reason about:
1. **Ownership is a hard switch** (East base XOR Macro base), but **blend is a
   soft mix** — and blend only applies in *one* of the two ownership states.
   So the same "send" knob does nothing when Macro owns the lane. That
   asymmetry is the core confusion.
2. **Destination addressing differs from source addressing** — ownership/East
   LOR are poly-bank indexed, sends/attens are 16-voice-slot indexed,
   resolved via VoiceResolver. Two parallel index schemes = where off-by-ones
   breed (see SANDS_CV_ROUTING_BUGS.md).

## A grand unifying pattern: "raw vs modulated input"

The user's framing — *make the Macro→East/Sands routing flexible by switching
between raw Macro-in and modulated-in* — is the simplification lever. Reframe
the two poly modules not as "two editors that fight over ownership" but as
**one poly editing surface fed by a selectable input stage**:

```
            ┌─────────────┐
 Macro CV ─▶│  input mode  │
            │  • RAW       │──▶ poly lane value ──▶ engine
 East edit ▶│  • MODULATED │
            └─────────────┘
```

- **RAW**: the lane value is Macro's global base directly (current "Macro owns").
- **MODULATED**: the lane value is East's per-voice edit, with Macro's CV
  blended in by the per-voice send amount (current "East owns + blend").

This is *exactly* what `combineLOR` already computes — but expressed as a
**single per-lane input-source switch** instead of an ownership latch whose
meaning changes the behaviour of a separate send knob. One concept ("where does
this lane's value come from: raw global, or my per-voice edit modulated by
global?") replaces the (ownership XOR) × (conditional blend) pair.

Crucially this also makes the **send knob always meaningful**: in MODULATED
mode it sets blend depth; in RAW mode the lane simply is the global value (the
send naturally reads as "100% global"). No dead controls.

## The three options

### Option 3 — keep three modules (status quo)
- **Pro:** Mono stays a clean standalone 6-lane editor (its simplicity *earns
  its place* — see below). Each module is independently optional.
- **Con:** the East↔Macro interaction is the whole problem; three panels to
  keep in lockstep (lane order, kit binds, geometry — the source of the recent
  churn). Highest surface area for drift + bugs.

### Option 2 — Mono + one poly module ("Poly")
Fold East and Macro into a single poly expander with the raw/modulated input
switch above.
- **Pro:** kills the inter-module mixer entirely — the "broad vs detailed"
  distinction becomes a per-lane *input mode* inside one module, not two
  modules negotiating ownership. One poly panel to maintain. Mono stays as-is.
- **Pro:** the send grids and ownership latches collapse into one row of
  per-lane mode switches + one depth knob per lane. Far less panel real estate
  (this also resolves the Macro lower-band space pressure from
  SANDS_PANEL_LAYOUT.md).
- **Con:** the combined poly panel is busier than East alone (it absorbs
  Macro's global base + send depth). Needs careful layout. But it's *one*
  busy panel instead of two interacting ones.
- **Con:** loses the ability to have Macro-without-East or East-without-Macro
  as distinct rigs — but those combinations (4, 7 in SANDS_COMBINATIONS.md)
  are exactly the ones whose semantics are hardest to explain, so losing them
  may be a feature.

### Option 1 — single module, paged
Everything in one module: a **global/settings page**, a **V1 page** that
exposes Mono's 2 extra lanes (Variation/Legato) on top of the 4 poly lanes,
and per-voice poly pages.
- **Pro:** one panel, one lane-order definition, one kit, one geometry — the
  entire class of cross-module drift bugs disappears. Patch state is one module.
- **Pro:** the "V1 has 6 lanes, V2-16 have 4" asymmetry becomes a natural
  page difference rather than a Mono-vs-East module difference.
- **Con:** loses the modular ethos — part of the appeal is dropping in only
  what you need (just Mono for a simple mono voice; add poly when you want it).
  A single paged module is more of a "mega sequencer" than an expander suite.
- **Con:** Mono's standalone simplicity (its strongest quality) is buried
  behind paging. A user who wants "just a 6-lane mono sequencer" now navigates
  pages past poly machinery they don't care about.

## Recommendation lean

**Mono earns its place and should stay standalone.** Its value is precisely
that it is *not* paged or poly — 6 lanes, one voice, direct, no interaction
semantics to learn. Burying it in a paged mega-module (Option 1) trades away
its best property. So Option 1's "one module" is attractive for drift-removal
but works against the thing that makes Mono good.

**The real win is Option 2's *idea* applied regardless of module count:**
collapse the East↔Macro mixer into a single per-lane **raw/modulated input
switch with one depth knob**. Whether East and Macro then remain two physical
modules or merge into one "Poly" module is secondary to fixing the *concept*.

So a sensible target:
- **Mono** — unchanged. The simple 6-lane standalone editor.
- **One poly story** — either merge East+Macro into a single "Poly" module, or
  keep them as two but redefine their relationship as input-stage (Macro =
  global source) → editor (East = per-voice, with a raw/modulated switch per
  lane). The latter keeps the modular drop-in feel while removing the ownership/
  send asymmetry.

East and Mono *do* overlap (East is a more flexible per-voice version of the
same lane model). That overlap is fine as long as the relationship is
"same lanes, more voices + an input switch," not "two editors contending for
ownership."

## Ownership UX (interim, regardless of consolidation)

The context-menu lane-ownership toggle (combination doc question B, now
implemented as right-click) is **fiddly**. Two improvements, both compatible
with the raw/modulated reframing:

1. **Make ownership a visible lane state, not a hidden menu.** Tint or border
   the editor lane by its input mode — e.g. RAW lanes drawn with a distinct
   hue/hatch (driven from global), MODULATED lanes in the normal per-voice
   colour. The user *sees* which lanes are global vs per-voice without opening
   anything. (This is the user's "change lane visuals to indicate ownership"
   instinct — endorsed.)
2. **Click target on the lane itself.** A small mode chip at the left end of
   each editor lane row (where the lane label sits) toggles raw/modulated on
   click — one click, on the thing it affects, with the state visible. Keeps
   right-click menu as the power-user fallback.

If East+Macro merge (Option 2), this chip *is* the entire ownership/send UI:
chip = mode, one depth knob = blend. The 2×2 send grids and separate latch
buttons go away.

## Open questions to resolve before acting

- Does the per-voice send need to be *per item* (Len/Off/Rot/Spr independently)
  or is **one blend-depth per lane** enough? The 2×2 grid exists because it's
  per-item; if per-lane depth suffices, the UI shrinks 4×.
- In MODULATED mode, is Macro's contribution additive (current: base + delta)
  or interpolative (crossfade global↔per-voice)? Interpolation may read more
  intuitively as a single "global ⟷ per-voice" knob.
- If merging to one poly module: does V1 (mono) editing belong there too, or
  stay exclusively in Mono? (Ties into SANDS_COMBINATIONS question A.)

## Sequencing

Do **not** start any of this until the current branch (lane order + kit binds +
accent wiring) is built and solid. This is a v-next structural decision; the
immediate path is still: build `refactor/sands-kit-macro`, convert Mono to the
kit, stabilise. The consolidation is the conversation *after* that.
