# PRINCIPLE: undo is user-only; plus a special class of REVERSIBLE automation (Rodney)

## Principle 1 (FIRM, worth a plugin-wide AUDIT): only USER actions go onto undo, NOT modulation
Anywhere a state change can be triggered by EITHER a user edit OR modulation, only the USER-triggered one
pushes undo history. Modulation/automation changes state but does NOT go onto the undo stack. (Standard:
you don't undo an LFO wobble; if you did, the undo stack fills with automation churn and can't undo YOUR
edits.) This is a STANDING ARCHITECTURAL PRINCIPLE for the whole plugin, not just CA.
AUDIT: worth auditing every module sometime -- it's easy to violate SILENTLY when user + modulation share
one commit path (as suspected in CA: applyPendingTransforms pushes TransformUndoAction per commit without
an obvious user-vs-mod gate; VERIFY + gate the push on arming class if needed).

## Principle 2 (the refinement): a SPECIAL CLASS of automation IS reversible -- Philox-invertible or small-cacheable
Automation is NOT undoable in general (Principle 1) -- EXCEPT where it's CHEAPLY REVERSIBLE BY CONSTRUCTION.
Really this is REVERSIBLE vs IRREVERSIBLE automation:
- GENERAL modulation = IRREVERSIBLE (can't reconstruct the pre-mod state; the CV did what it did, no
  inverse, no cache) -> NOT undoable, stays out (Principle 1).
- SPECIAL automation = REVERSIBLE, by one of two enabling properties:
  1. INVERTIBLE GENERATOR (Philox is a keyed BIJECTION): reverse = re-derive the earlier draw EXACTLY
     (at(N-1)). Undo-by-reversing-the-generator -- FREE, no stored history (Philox IS the inverse).
     => reversible dice. UNBOUNDED reach (any index, no storage -- it's math).
  2. SMALL CACHEABLE STATE (CA): the state is small enough to cache the phrase-trajectory (~6-12KB for
     32-64 phrase-states). Undo-by-cached-state. => CA true-reverse. BOUNDED reach (only as far as the
     cache holds; beyond that it's like general automation -- gone).
If automation has NEITHER (large state + non-invertible process) -> irreversible -> no reversal, stays out
of undo. If EITHER -> qualifies for reversal affordances.

### Why dice-reverse + true-reverse are SEPARATE from user-undo
They are the REVERSAL AFFORDANCES for the reversible-automation class -- NOT user-undo (Principle 1 keeps
automation out of that), but a PARALLEL reversal possible ONLY BECAUSE the automation is cheaply invertible
(Philox) or cacheable (CA small state). General automation can't have these (not reversible); the special
class can (it is). This is WHY these mechanisms exist as their own thing distinct from Rack undo.

### The two reach profiles (nice contrast)
- Philox reversibility = FREE + UNBOUNDED (a bijection; go arbitrarily far back for zero storage).
- CA cache reversibility = CHEAP + BOUNDED (storage; reaches back the cache depth, 32-64 phrases, then the
  history is gone).
Both qualify for the special class, by DIFFERENT mechanisms with DIFFERENT reach. The cache-size limit is
the honest boundary on the CA side.

## Combined statement
- USER edits -> undoable (Rack history).
- MODULATION/automation -> NOT on the undo stack (Principle 1) ...
- ... UNLESS it's REVERSIBLE BY CONSTRUCTION (Principle 2): Philox-invertible (dice, free/unbounded) or
  small-cacheable (CA, cheap/bounded) -> gets its own reversal affordance (dice-reverse / true-reverse),
  parallel to user-undo, not on the user-undo stack.
- General irreversible automation -> neither undoable nor reversible.

Cross-ref: CA_DICE_COUNTER_MODEL (the CA-specific working: user-vs-modulation classes, dice-reverse via
signed Philox, true-reverse via cached phrase-trajectory, the possible modulation-pollutes-undo bug to
verify), PHILOX_KEY_DERIVATION (the bijection that makes dice reversibility free/unbounded), the true-
reverse buffer-size note (the CA cache = the bounded reach). AUDIT applies plugin-wide.
