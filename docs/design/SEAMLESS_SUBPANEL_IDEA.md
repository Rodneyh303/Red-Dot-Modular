# Seamless pop-out subpanels — the Tessera-inspired panel arc

Status: VISION / direction. The next major arc AFTER the engine work
(reverse/position landed; CA5; the reverse completions). Not scheduled.

## The inspiration (Rodney, Tessera Roll)

Tessera's Roll module pops out an editor side panel of a chosen HP that flows
SEAMLESSLY — drawn as ONE window, no visible module seam. Apply the same to
dot.modular's multi-expander modules: instead of a sprawl of separate docked
expanders, pop out a seamless, runtime-configurable panel.

## The organizing criterion (the clean line through it)

**The seamless pop-out suits expander modules that only belong to ONE main
module.** A single-parent expander has no independent identity to protect — it
only ever attaches to the one host — so folding it into a seamless pop-out of
that host loses nothing and the seamlessness is pure gain. This also dissolves
the jacks question: moving a single-parent expander's PORTS into the grown host
is a contained change (no other host, limited patch-compat surface). A
general-purpose / multi-host / standalone module is the opposite — it needs its
modular I/O and the freedom to live anywhere — so it STAYS a separate dockable
module.

Per-module test: "does this expander ever make sense attached to anything but
its host, or used on its own?" No → pop-out candidate. Yes → stays a module.
(Classification is Rodney's call in front of Rack, per module.)

## The seed already exists

Distribution is already a subpanel of Sands East — the proof-of-concept pattern,
and a single-parent expander, so a prime pop-out candidate. The arc generalizes
that one pattern (+ Tessera-style resize + the seamless one-window draw + a
loadout choice).

## Candidate scopes (classification pending, Rodney's call)

- Monsoon: pop out a seamless panel with a CHOICE of Sikit / Colonnades / Duo,
  plus Raffles, plus 1-2 Interchange; potentially Shophouse — for whichever of
  these are single-parent.
- Distribution (already a Sands East subpanel) → seamless pop-out.
- Straits subpanels → same treatment.

## The two implementation forks (the criterion above mostly resolves them)

1. UI-only vs jacks: single-parent → fold in, jacks and all; multi-host → stays
   modular. (The criterion decides this.)
2. One resizable module vs seam-hidden docked modules: (a) true Tessera — one
   ModuleWidget grows (box.size.x), truly seamless, all I/O in one module; (b)
   separate modules that abut with aligned backgrounds / no gap — fakes the
   one-window look, preserves modular I/O + patch compat, fits the existing dock
   machinery. For single-parent expanders (a) is natural; multi-host ones that
   stay separate can still use (b) to look continuous.

## Reuse

The subpanel family + role-walk / left-link / drag-resnap dock machinery already
exists (Macro-send, Distribution, Phase-Dist). The arc builds on it, not
greenfield.

## VCV gotchas (known-solvable — Tessera proves it)

Runtime box.size.x resize: a panel background that scales to width, the width
saved in the patch, neighbour push, port/cable anchoring when the body grows,
the loadout choice persisted, and migration for patches that used a now-folded
single-parent expander as a separate module.

## Sequencing

Engine arc → THIS panel arc → runtime ring. This panel work is ALSO the enabler
for the runtime 16/32/64 step idea (SIXTYFOUR_STEP_IDEA.md): the pop-out-WIDER
Sands that gates 64 steps is a special case of this same seamless-subpanel
capability. So the panel arc delivers pop-out-wider-Sands and the configurable
loadouts; the runtime ring rides on top.

## Names / identity survive the fold

Keep the subpanel names (Raffles, Interchange, Colonnades, Sikit, Shophouse, …).
The seamless draw removes the module SEAM, not the identity. Each folded subpanel
keeps its name and visual style as: a titled band / section header within the one
window; the loadout CHOICE label when picking what to pop out ("show Raffles");
a context-menu entry; and the manual. The Singapore-geography naming is deliberate
brand identity — seamless does not mean anonymous.

The ONE change for a folded single-parent subpanel: it stops being an
independently BROWSABLE VCV module — reached by popping it out of the host, not by
searching the library and dragging it in. The name moves from "a module in the
browser" to "a named loadout/section of the host." That is fine precisely because
it is single-parent. Useful tell: if you ever want one to stay searchable and
placeable on its own, that is the signal it is NOT single-parent and should remain
a separate module.

## The loadout is slots: some EXCLUSIVE (one-of-N), some ADDITIVE

The pop-out loadout is not an arbitrary set of panels — it's a small set of
SLOTS:
- EXCLUSIVE slot (pick exactly one of N): e.g. one of {Sikit, Colonnades,
  Colonnades Duo} — same role at different sizes, so never more than one. The
  region is reserved and swaps content (and width) when the choice changes.
- ADDITIVE slot (add up to M): e.g. Raffles (0/1), Interchange (0/1/2),
  possibly Shophouse.
So the shape is always "host (Monsoon) + one choice-slot + any additives"; the
pop-out grows/shrinks to the chosen loadout but the structure is fixed. That
constraint SIMPLIFIES the resize/draw: finite named loadouts, an exclusive region
that swaps rather than arbitrary panel combinatorics.
