# CC FIX  move PhaseDistStore off the Distribution expander onto the phase subpanel

Build on feat/phase-duals-gaps.

## THE PROBLEM
The phase-distortion sets live in a PhaseDistStore that is a MEMBER of SandsDistributionExpander
(SandsDistributionExpander.hpp:126 phaseDistStore). The authoring subpanel SandsPhaseDistSubpanel edits that
store, resolved rack-wide as the lowest-ID SandsDistributionExpander. So the phase subpanel does NOTHING until a
Distribution expander is added  a surprise dependency between two modules that share only the distribution MATHS,
not a reason to share a MODULE. Distribution modifies probabilities; phase distortion modifies transport.

## THE FIX  the subpanel owns the store
Move the eight-set PhaseDistStore to be a member of SandsPhaseDistSubpanel. The subpanel becomes self-contained,
like the standalone PhaseDistortion module already is (it carries its own embedded PhaseDistStore). No Distribution
expander is required to author or use phase sets.

Steps:
1. Add `redDot::PhaseDistStore phaseDistStore;` as a member of SandsPhaseDistSubpanel; remove it from
   SandsDistributionExpander.
2. The subpanel's knob bindings (SandsPhaseDistSubpanel.cpp, the StoreKnob<SandsDistributionExpander> grid) rebind to
   the subpanel's own store: StoreKnob<SandsPhaseDistSubpanel> (or the subpanel's store directly). The 8x8 grid and
   the curve editor read/write the subpanel's store.
3. resolveStoreOwner: DELETE it. The owner is `this`. storeOwnerLive is always true when the subpanel exists; the
   "NO STORE IN REACH" footer message is removed (it can no longer happen).
4. Monsoon reads the phase sets from the subpanel, not the Distribution expander. Update whatever Monsoon/engine
   path pulls the set-by-index (the PhaseDistBlockReader / snapshot source) to resolve the subpanel as the source.
   If the design wants Monsoon to pull by index, the subpanel is the rack-wide source the same way the Distribution
   expander was  but keyed to the subpanel model, and WITHOUT the surprise dependency (the subpanel is the phase
   authoring surface, so requiring IT is not a surprise).
5. The block-consistency snapshot (per-block, mutex-guarded) moves with the store to the subpanel. Keep the
   consistency rule intact.

## WHAT STAYS
- The standalone PhaseDistortion module is unchanged (it already owns its store).
- The transform primitive, gap/atom/dual maths, warp, and the curve plot are unchanged.
- SandsDistributionExpander keeps its distribution role (probabilities); it simply no longer hosts the phase store.

## GATES
- The phase subpanel authors and plots phase sets with NO Distribution expander in the rack.
- Adding/removing a Distribution expander has NO effect on the phase subpanel.
- Monsoon reads the correct set-by-index from the subpanel; identity sets are bit-identical to today.
- Block consistency preserved (the per-block snapshot test still passes).
- run_all green on both SDK legs.
- No change to the Distribution expander's probability behaviour.

## NOTE (principle)
Expanders hold state and do little work. The phase store is the subpanel's own state; it should live there, not on
an unrelated expander. This removes the cross-module dependency that confused connection debugging.
