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

## A grand unifying pattern: per-voice send taps PRE or POST Macro's global attenuverters

The user's framing — *raw vs modulated* — is about the **tap point** of each
per-voice mix-in send on Macro's own global CV path, NOT about East↔Macro
ownership. It's a signal-routing choice local to Macro.

### Macro's current global CV path (per lane, per item)

In `MonsoonSandsManager.cpp::publishGlobal` → `applyMacroCV`:

```
cvValue      = base + (CVin/10) · globalAtten · (hi−lo)     // base = knob, no CV
macroCVDelta = cvValue − base = (CVin/10) · globalAtten · (hi−lo)
```

So `macroCVDelta` is the CV contribution **already scaled by Macro's global
left attenuverter**. Then in `combineLOR` the per-voice send scales it *again*:

```
blend = macroCVDelta · perVoiceSend          // = CVin · globalAtten · send
```

→ The per-voice send currently taps **POST** the global attenuverter: global
atten and per-voice send **multiply**. Today there is no choice; it's always post.

### The proposed switch: PRE vs POST per send (or per lane)

```
 Macro CVin ─▶─┬─────────────────────────────▶ (PRE / "raw" tap)
               │
            globalAtten (left attenuverter)
               │
               └─────────────────────────────▶ (POST / "modulated" tap)
                                                      │
                                   per-voice send ◀───┴── tap selected here
```

- **POST ("modulated", current):** send scales `CVin · globalAtten`. The global
  attenuverter sets overall depth for the lane; each voice's send is a fraction
  *of that already-attenuated* signal. Global knob and send compound.
- **PRE ("raw"):** send scales the **raw** `CVin · (hi−lo)`, bypassing
  globalAtten. Each voice's send becomes an independent full-range depth on the
  raw input; the global attenuverter then only affects Macro's *own*
  displayed / owned-lane value, not what the per-voice sends receive.

### Why PRE is attractive

- **Decouples the two knobs.** Post-mode means turning the global atten down
  quietly shrinks every voice's send response — a hidden interaction. Pre-mode
  lets the global atten shape Macro's own lane while each voice's send is its
  own clean depth on the source CV.
- **Per-voice sends become real independent depths**, not fractions-of-a-fraction.
- It's the natural answer to "I want this voice to get the *full* CV regardless
  of where I've set the global trim."

### Cost

- One bit of state per tap point (or per lane, if we don't need per-item):
  PRE/POST. Smallest version: a single per-lane (or even per-module) switch.
- `applyMacroCV` must publish **two** deltas — `rawDelta = (CVin/10)·(hi−lo)`
  and the existing `attenDelta = rawDelta·globalAtten` — and `combineLOR`
  picks which to multiply by the send based on the switch. Cheap; the value is
  already computed, we just stop folding the atten in unconditionally.

### Relationship to ownership

This is **orthogonal** to the East-vs-Macro ownership switch — ownership still
chooses whether a lane's *base* is East's per-voice edit or Macro's global
value. PRE/POST only changes what the per-voice **send** modulates on top. They
compose: e.g. East owns the base, per-voice send blends in the raw (pre-atten)
Macro CV. (Earlier draft of this doc mis-stated raw/modulated as an ownership
reframing; corrected here.)

## The three options

### Option 3 — keep three modules (status quo)
- **Pro:** Mono stays a clean standalone 6-lane editor (its simplicity *earns
  its place* — see below). Each module is independently optional.
- **Con:** the East↔Macro interaction is the whole problem; three panels to
  keep in lockstep (lane order, kit binds, geometry — the source of the recent
  churn). Highest surface area for drift + bugs.

### Option 2 — Mono + one poly module ("Poly")
Fold East and Macro into a single poly expander.
- **Pro (the honest one):** kills the two-panel **lockstep maintenance** — lane
  order, kit binds, geometry no longer have to be kept identical across East and
  Macro. This coordination is the actual source of the recent churn, so this is
  a real, concrete win. Mono stays as-is.
- **Pro:** the send grids + ownership control can collapse into one compact
  per-lane control set (lane chip + one depth knob), recovering panel space
  (also eases the Macro lower-band pressure from SANDS_PANEL_LAYOUT.md).
- **Con:** the combined panel is busier than East alone (absorbs the global base
  + sends). One busy panel instead of two interacting ones.
- **Con (do NOT undersell this):** you lose **Macro-only** and **East-only** as
  lightweight drop-in rigs. To get either in a merged module you instantiate the
  full Poly module and simply don't use half of it — heavier in the rack for
  what were simple jobs, which cuts against the modular drop-in ethos that makes
  Mono's standalone simplicity good.

**Correction to an earlier draft:** a previous version justified the merge with
"combinations 4 and 7 are the hardest to explain, so losing them may be a
feature." That was wrong on both counts:

- **Combination 4 (Macro only) is one of the *cleanest* configs, not the
  hardest.** No East ⇒ no ownership, no XOR, no two-editor contention: Macro's
  global base is the only LOR source and the per-voice sends just vary how much
  each voice deviates. That's a legitimate, easy-to-explain rig — "16 poly
  voices sharing a global character, spread per-voice from one place." It is
  exactly the kind of lightweight standalone module the suite should keep.
- **Combination 7 (East + Macro, no Mono) is where the genuine complexity
  lives** (live per-lane-per-voice ownership + conditional blend) — but merging
  does **not** retire that complexity, it *internalises* it. The "global base vs
  this voice's own edit" choice is the load-bearing feature that distinguishes
  broad-strokes from detailed-strokes poly; you can simplify its UI but you
  cannot delete the concept by merging. So 7 is not an argument *for* merging.

So merging is **not** justified by "dropping confusing combinations." Its only
honest justification is lockstep-maintenance reduction, weighed against the loss
of the Macro-only / East-only standalone rigs.

Note: the **PRE/POST send tap** above is independent of this — it applies
whether the global source and editor are one module or two.

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

**The real win is fixing the *concept*, somewhat independently of module count.**
Two separate simplifications:
1. **Make the per-voice send tap selectable PRE/POST the global attenuverter**
   (above) — decouples the global trim from the per-voice send depths, which is
   the concrete "raw vs modulated" the suite actually needs.
2. **Reduce the ownership/send UI** to a per-lane mode chip + one depth knob
   (below), whether or not East and Macro stay separate modules.

So a sensible target:
- **Mono** — unchanged. The simple 6-lane standalone editor.
- **Concept fixes first (module-count-agnostic):** the PRE/POST send tap and the
  slimmer per-lane ownership UI. These remove most of the *felt* complexity
  whether or not the modules merge — so they should happen **before** any
  merge decision, and may well make the merge feel unnecessary.
- **Then, the module-count call is a narrow maintenance-vs-modularity trade:**
  - *Keep two* (Mono + East + Macro): preserves Macro-only and East-only as
    clean lightweight drop-ins (Macro-only in particular is one of the easiest
    rigs to explain). Cost: ongoing lockstep upkeep between the two poly panels.
  - *Merge to one Poly*: removes that lockstep upkeep (the real churn source).
    Cost: loses those lightweight standalone rigs; the merged panel is busier.

**Lean:** do the concept fixes, keep three modules, and only revisit merging if
lockstep upkeep is *still* painful afterwards. The kit conversion already
attacks the upkeep cost from the other side (geometry lives in the gen scripts,
not duplicated in C++), which weakens the merge's main remaining justification.
The merge trades away genuinely-useful standalone configs to solve a maintenance
problem the kit work is already shrinking — so it should be the last resort, not
the default.

## Ownership UX (interim, regardless of consolidation)

The context-menu lane-ownership toggle (East base vs Macro base per lane per
voice; combination doc question B, now right-click) is **fiddly**. Two
improvements:

1. **Make ownership a visible lane state, not a hidden menu.** Tint or border
   the editor lane by its base source — e.g. Macro-owned lanes drawn with a
   distinct hue/hatch (value comes from global), East-owned lanes in the normal
   per-voice colour. The user *sees* which lanes are global vs per-voice without
   opening anything. (The user's "change lane visuals to indicate ownership"
   instinct — endorsed.)
2. **Click target on the lane itself.** A small chip at the left end of each
   editor lane row (by the lane label) toggles ownership for the current voice
   on click — one click, on the thing it affects, state visible. Keep the
   right-click menu as the power-user fallback.

(The PRE/POST send tap is a separate, smaller control — likely a per-lane or
per-module switch near the global attenuverters, not on the editor lane.)

If East+Macro merge (Option 2), the lane chip + one depth knob per lane *is*
the entire base-source + send UI; the 2×2 send grids and separate latch buttons
go away.

## Open questions to resolve before acting

- Does the per-voice send need to be *per item* (Len/Off/Rot/Spr independently)
  or is **one depth per lane** enough? The 2×2 grid exists because it's
  per-item; if per-lane depth suffices, the UI shrinks 4×.
- **PRE/POST granularity:** is the tap switch needed per-item, per-lane, or just
  one per-module? Coarser = simpler UI. Per-lane is probably the sweet spot.
- When East owns the base, is Macro's send contribution additive (current:
  base + delta·send) or interpolative (crossfade East-edit ↔ Macro-CV)?
  Interpolation may read more intuitively as a single "mine ⟷ global" knob.
- If merging to one poly module: does V1 (mono) editing belong there too, or
  stay exclusively in Mono? (Ties into SANDS_COMBINATIONS question A.)

## Sequencing

Do **not** start any of this until the current branch (lane order + kit binds +
accent wiring) is built and solid. This is a v-next structural decision; the
immediate path is still: build `refactor/sands-kit-macro`, convert Mono to the
kit, stabilise. The consolidation is the conversation *after* that.
