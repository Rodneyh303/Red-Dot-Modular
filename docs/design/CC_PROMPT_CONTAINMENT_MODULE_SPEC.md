# SPEC — Containment module (the articulation-envelope routing) — a forest-constrained CA sibling

Design authority: ARTICULATION_ENVELOPE_VS_CORRELATION.md. This is a NEW module (sibling of ChangeAlley/
CA) that routes CONTAINMENT (articulation-envelope nesting), NOT correlation. It COPIES CA's shape (pin
matrix, phrase-boundary transform application, the Temasek-style transform control set) but with ONE
defining difference: its structure is a FIXED ACYCLIC FOREST, and editing + transforms must PRESERVE that
(never create a cycle; always a forest). This is a DESIGN SPEC for future build, not an immediate task;
sequence after the decision-manager (containment needs the recursive gate-decision machinery to consume
it). Build on lane-expander-refactor when scheduled.

## WHY A SEPARATE MODULE (the settled conclusion — do not re-litigate)
Containment canNOT ride CA: CA correlation is cycle-SAFE (reads pre-existing draws) so CA freely modulates
into cyclic states; containment is cycle-FATAL (composes derived gates up a chain — non-terminating on a
cycle, can't be stale-delayed). So containment gets its OWN routing, structurally acyclic, CA-untouched.
Its transforms are drawn ONLY from the class that is CLOSED over acyclic forests (endomorphisms on the
forest space) → no cycle can EVER arise, no per-transform cycle check needed — safe by ALGEBRAIC CLOSURE.
This makes it a STRUCTURAL-modulation module (modulates the HIERARCHY) distinct from CA's VALUE-correlation
modulation.

## THE STATE — a single-parent acyclic forest over the voices
- ONE pin per voice (the destination): container[v] ∈ { NONE | some voice u }. Single-parent (each voice
  has at most one container) → with acyclicity, a FOREST (rooted trees + singletons).
- Copy CA's PIN MATRIX UI (MonsoonChangeAlleyV2 pin grid) but ONE routing plane (containment), ONE pin per
  destination row. (CA has 3 correlation planes; this module has 1 containment plane.)
- INVARIANT (enforced at every edit AND every transform): single-parent + ACYCLIC. The structure is ALWAYS
  a valid forest.

## EDITING — pin edits must BLOCK cycles (only create forests)
When the user sets container[v] = u:
- REJECT if u == v (self-containment).
- REJECT if u is a DESCENDANT of v in the current forest (would close a cycle: v ⊂ … ⊂ u ⊂ v). Check =
  walk up from u via container[] pointers; if you reach v, reject. Single-parent makes this O(depth).
- Otherwise accept (it's a valid forest edge).
Setting container[v] = NONE always valid (detaches v to a root). The UI should SHOW the rejection (can't
pin — would cycle), not silently fail. (Same spirit as CA rejecting invalid pins, but here the rule is
acyclicity, which CA correlation does NOT enforce.)

## THE TRANSFORM CLASS — ONLY forest-closed transforms (endomorphisms on the forest space)
Copy CA's transform-apply + phrase-boundary machinery (ChangeAlleyTransforms.hpp verbs, CAPendingOrder.hpp
chronological pending, apply-at-phrase-boundary), BUT the transform SET is restricted to operations that
map forest → forest BY CONSTRUCTION (so no cycle can appear, no post-transform check needed):
  SAFE (build these):
  - SIMILITUDE / RELABEL: permute WHICH VOICES hold which roles, shape PRESERVED (a forest automorphism —
    rotate/shift the role assignment across voices; the nesting SHAPE is identical, the voices rotate
    through it). "The hierarchy rotates through the voices." Closed: relabeling a forest is a forest.
  - TOPOLOGICAL INVERSION: reverse the hierarchy orientation (root↔leaf flip; 2⊂3⊂4 → 4⊂3⊂2). A reversed
    tree is a tree. "Turn the nesting inside out." Closed.
  - DEPTH COLLAPSE / EXPAND: flatten a subtree toward its root (collapse) or restore nesting depth
    (expand). Changes depth, not acyclicity. "Collapse/expand the nesting." Closed.
  - SUBTREE SWAP: exchange two sibling subtrees. A forest automorphism. Closed.
  - ATTACH-TO-ROOT GRAFT: move a subtree to be a child of a root (or to NONE). Always a forest (attaching
    to a non-descendant; root is never a descendant of a node in another tree). Closed.
  UNSAFE (do NOT build as free transforms — they are re-linking ops that CAN create cycles):
  - arbitrary re-parent / graft-to-descendant / chain-rotation-by-RE-LINKING. These need a per-op cycle
    check and break the closed-algebra guarantee — exclude from the transform set (editing already covers
    deliberate re-parenting with its cycle check).
PRINCIPLE: a containment transform is admissible IFF it is an endomorphism on acyclic forests. The safe
list above is that class; verify each is closed (a quick property test: apply to random forests, assert
result is always a forest) — the test is the guarantee.

## TRANSFORM APPLICATION — at PHRASE BOUNDARY (copy CA)
Transforms arm (pending) and APPLY at the phrase boundary, exactly like CA (CAPendingOrder chronological
pending, applyPendingTransforms at the boundary). Between boundaries the forest is held. (Consistent with
containment = the stable skeleton: it changes only at phrase seams, and only via forest-closed transforms.)
Triggers: same mechanism as CA transform triggers (buttons/CV arm a pending transform; fires next phrase).

## WHAT CONSUMES IT (the decision manager — note, not built here)
The forest is consumed by the gate-decision resolution: a voice's gate is clamped within its container's
resolved gate, recursively up the tree (gate(v) = clamp(own_gate(v), gate(container(v)))). The acyclic
forest guarantees this recursion is well-founded (base case = a root; finite depth). That recursion is the
DECISION MANAGER's job (MONO_SPECIAL_IN_LOGIC_AUDIT.md) — this module only PRODUCES the forest; the decision
manager CONSUMES it. Build the module's routing+transforms first; the gate-clamp recursion comes with the
decision-manager work.

## DONE (when built) =
- Containment module: 1-pin-per-voice forest state; CA-style pin matrix UI (one plane); edit rejects
  cycles (O(depth) ancestor walk) → always a forest.
- Forest-closed transform set (similitude, inversion, depth collapse/expand, subtree swap, root-graft),
  each property-tested closed-over-forests; phrase-boundary pending application (copy CA machinery). NO
  unsafe re-linking transforms.
- Tests: edit-cycle-rejection; each transform forest-closed (random-forest property test); phrase-boundary
  timing. run_all.sh green.
- (gate-clamp consumption = the decision-manager work, separate.)

## NOT THIS SPEC
- The gate-clamp recursion / decision manager (consumes the forest; separate).
- Follow-CA containment (RETIRED — can't ride modulatable CA). Nesting depth is AVAILABLE (fixed forest
  makes it safe) but whether to expose it is Rodney's call.
- Dynamic containment via non-forest modulation (the priority-order escape hatch) — not needed; the
  forest-closed transforms ARE the safe dynamism.
