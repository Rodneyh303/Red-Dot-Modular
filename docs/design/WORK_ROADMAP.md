# Work roadmap — plan of record

Status: **living order-of-work.** Not module design (that's MODULE_NAMING_AND_ROADMAP.md);
this is the SEQUENCE of workstreams and why they're ordered as they are. Structural work
before polish, so we refine a settled foundation rather than repaint a moving target.

## Order

1. **De-paramming (in progress).** Move display-mirror params to the store.
   - Macro: **DONE** — attenuverters, spread, taps, LOR, direction, sends all store-backed.
   - Next: `config()` → 0 audit on Macro, then **Mono Sands (54)**, then **East (38)**.
   - Recipe + traps: see the "De-param playbook" in MVC_UNIFICATION.md. East is partly
     pre-migrated (DirCell store callbacks + setLorBase already exist); Mono is the bigger
     genuine unknown.

2. **Lock mode.** After de-param, because lock mode touches engine/latch state and wants a
   stable param model underneath. (If any part is purely visual it can slip toward polish;
   decide per-piece which side of the line it's on.)

3. **Undo for LOR + direction (optional).** A UNIFORM cross-module pass or nothing — doing it
   per-module reintroduces exactly the inconsistency the de-param removed. "One clean session
   or leave it," not a chip-away item. See the undo backlog note in MVC_UNIFICATION.md.

4. **Panel refinement + polish.** The surface catches up to the depth. Only once the
   behaviour underneath is settled.

## The lens to hold throughout: horizontal channel budget

We reason about voices VERTICALLY — 16 rows, the poly bank, spread collapsing toward mono.
The HORIZONTAL budget is the other axis and easy to under-plan: the 16 steps, the CV/gate
outputs, how many independent streams LEAVE the module, and what a downstream patch can do
with them. A sequencer can be voice-rich yet feel narrow if everything funnels through too
few outputs, or sprawl if every lane demands its own cable. Change Alley is partly a
horizontal-budget instrument — correlating voices to reduce randomness is about relationships
ACROSS the channel width, not just depth per voice. Keep this explicit as panels are refined:
panel space and output routing are where the horizontal budget becomes physical.

## North star

Mono primacy, "the little red dot", correlation-as-composition — the identity is coherent
already. The remaining work is making the surface match the depth that's there. Gradual,
compounding improvement, not a rewrite.
