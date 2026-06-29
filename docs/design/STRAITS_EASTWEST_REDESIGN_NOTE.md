# Straits East/West redesign — concept note (FOR LATER)

Status: **idea capture, not a spec, not scheduled.** Recording the direction so it isn't
lost; the detailed design happens later, after the Sands topology consolidation. Cross-refs:
`SANDS_TOPOLOGY_RESOLVER_PLAN.md` (ownership), `SANDS_LANE_INDEX_AUDIT.md` (the four lane
conventions), `CODEBASE_REFACTOR_REVIEW.md` (West is currently a half-wired ghost — B0).

---

## The problem with today's East/West

One expander pair conflates several concerns:

- `MonsoonStraitsEastExpander` (base, live) — carries the poly OUTPUTS:
  `POLY_GATE_OUT_1..7`, `POLY_CV_OUT_1..7`, `POLY_ACCENT_OUT_1..7`.
- `StraitsEastSandsVisual` (the tabbed editor, ~942 lines, live) — carries, all in one
  surface: base LOR levels, per-voice CV atten/send columns, ownership cells, AND the V1
  (mono) editor.
- West (`MonsoonStraitWestExpander` live; `StraitsWestSandsVisual` ghost — not registered,
  never attaches) was meant to mirror East for voices 9–16.

So a single module is simultaneously: **base value editor + poly output jacks + CV
modulation routing + mono path** — four jobs. This is *why*:

- the editor is 942 lines and hard to reason about,
- poly vs mono is muddy (V1 = "mono slot" wedged into a poly-shaped surface; the
  per-voice STORAGE banks vs the editor READ addressing are two index schemes — the
  documented off-by-one breeding ground, `SANDS_LANE_INDEX_AUDIT.md`),
- ownership/lock/CV logic had to special-case "voice 0 is different" everywhere (the whole
  Mono+Macro saga).

---

## The proposed direction (user sketch)

Split the one conflated expander into **concern-aligned modules**:

1. **Base + poly outs expander.** Holds the base per-lane levels and the poly output jacks.
   Knobs laid out to **resemble Monsoon's own REST / ACCENT** sections — i.e. the base
   editor looks and feels like the parent module's controls, not a bespoke grid. This is
   the "what are the levels, and here are the voices out" module.

2. **CV-modulation expander.** Holds the CV inputs + attenuators + (sends?) — the
   modulation routing that currently clutters the visual editor's per-voice columns. The
   base module stays clean; modulation is opt-in by adding this.

3. **(Maybe) mono-outs expander.** A dedicated surface for the MONO path outputs, so the
   mono signal path stops being "voice 0 of a poly thing" and becomes its own first-class
   module with its own outs.

Guiding principle the user named: **rethink poly vs mono CV and outs cleanly.** Right now
they share storage/addressing and breed off-by-ones. The split should make mono and poly
*structurally separate* — different modules, different jacks, no shared "slot 0 is special"
indexing.

---

## Why this is the right shape (connects to the topology work)

- The `SandsTopology` resolver (in progress) already separates V1/mono from poly voices in
  the *decision* layer (`owner(voice, lane)`, voice 0 = mono). This module split is the
  *physical* counterpart: if mono outs live on their own module, the resolver's
  "voice 0 is special" branches map onto a real module boundary instead of a wedged slot.
- Concern separation shrinks the 942-line editor into focused surfaces, each testable.
- "Base looks like Monsoon REST/ACCENT" reduces the bespoke-grid surface area that has its
  own lane-index convention (one fewer convention to bridge).

---

## Open questions to settle before designing (not now)

1. **Mono outs: separate module, or part of the parent Monsoon?** The parent already has a
   mono path; does a mono-outs expander duplicate or extend it? Decide whether mono is
   "Monsoon's own" vs "a Sands expander".
2. **CV expander granularity:** one CV module for all voices, or East-range / West-range
   CV modules paralleling the base split? And do *sends* (Macro blend) live with CV, or
   stay with ownership on the base?
3. **Ownership cells — which module hosts them?** Ownership is per (voice, lane). If base
   and CV are separate modules, the owner cell logically belongs with the base (it governs
   whose *value* wins), but the CV module needs to read it. The topology resolver is the
   clean seam: both modules consult the topology, neither owns the flag twice.
4. **West revival vs retirement:** today West is a ghost (B0). This redesign is the moment
   to decide — does West come back as the voices-9–16 counterpart of the new base module,
   or is the voice range folded into one expander with a count param? The chain ordering
   (East then West) and the 7-vs-15 voice cap (`polyOutCap`) interact here.
5. **Backward compat:** existing patches reference the current East expander's params/IDs.
   A split changes the param map → patches break unless migrated. Decide whether this is a
   clean break (new module IDs, "v2") or a migration path.
6. **Expander chain semantics:** more modules in the chain = more attach-order combinations
   = more topology `Config` states. The resolver must enumerate them (it already
   enumerates all reachable; this just adds members). Keep the "named config, never a
   hand-built flag combo" discipline so the chain growth doesn't reintroduce the
   combination-bug class.

---

## Sequencing

This is a **post-topology** project. Doing it before the topology consolidation would mean
designing module boundaries against the current tangled ownership logic; doing it after
means the resolver already cleanly separates poly/mono and ownership, so the module split
follows the seams the resolver exposes. Order: finish topology (steps 3–6) → then revisit
this with the resolver as the contract between the new modules.

No action now. Captured for the redesign session.
