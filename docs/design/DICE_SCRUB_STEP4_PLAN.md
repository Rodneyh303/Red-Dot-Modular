# Step 4: retire stored A/B arrays + the cached-A/B snapshot system

Precondition MET: the scrub recompute re-derives patterns from the counter, and persistence saves
the full scrub state -- seed key (rhythm/melodySeedFloat), counter (drawCtrR/M), slew (slLatchedR/M),
mix (scrub pos). So LockedA/CandB are redundant for BOTH runtime and load: a loaded patch reconstructs
its pattern from counter+seed+slew+mix. Safe to remove.

## Scope (3 files, ~115 refs) -- PURELY SUBTRACTIVE
1. PatternEngine.hpp (22 refs): remove the 20 arrays --
   rhythm/variation/legato/accent LockedA+CandB (mono) + polyRhythm/polyAccent (poly) + melody/octave
   LockedA+CandB + polyMelody/polyOctave. Plus any cRhythmA/B... cached-snapshot arrays.
2. PatternEngine.cpp (64 refs):
   - redrawRhythm/redrawMelody: the step()=a+slew*(unit()-a) population of CandB from LockedA and the
     `first` branch that draws A/B -- REMOVE. redraw still must ADVANCE THE COUNTER (advanceRhythmDraw)
     and set rhythmMode/flags; that stays. The A/B fill is dead (recompute ignores it).
   - The mode-switch cached-A/B snapshot block (cRhythmA/B save at redraw ~545, restore at ~560,
     rhythmABCached flag): OBSOLETE under scrub. The "lossless A/B snapshot to preserve the slew
     morph position across a mode switch" is unnecessary -- the counter+slew ARE the morph position
     and are preserved across mode switch trivially (counter unchanged). Replace the save/restore
     with: on mode switch, nothing needed (or at most re-run recomputeEffective from the current
     counter). REMOVE the cache-A/B path.
   - recomputeEffective no longer references LockedA/CandB (already true post step 3) -- confirm.
3. MonsoonPersistenceManager.cpp (29 refs): remove the A/B save+load (committed A arrays, cand B,
   cached snapshots). Keep drawCtr/seed/slew/mix persistence (that IS the scrub state). Old dev
   patches lose saved A/B but re-derive from counter+seed on load -- acceptable (unreleased).

## Sub-steps (commit each GREEN immediately -- reset-safety)
4a. Remove the redraw step()/first A/B population + the 20 arrays' WRITES; make redraw just advance
    counter + set flags. Keep array decls momentarily if needed to compile, then...
4b. Remove the cached-A/B snapshot system (cRhythmA/B, rhythmABCached, save/restore).
4c. Remove the 20 array declarations (now unreferenced) + A/B persistence.
Test after each; the pattern must still reproduce from the counter (verify a seeded pattern
unchanged). Expected behaviour: IDENTICAL to now (A/B were already dead for output post step 3);
this is dead-code removal + persistence slimming.

## Risk
Purely subtractive but LARGE and spread across redraw + mode-switch + persistence. The mode-switch
cache removal is the subtle part (ensure mode switch still yields the right pattern via counter, not
the removed snapshot). Verify in Rack: switch rhythm mode (realtime<->clocked) mid-pattern, confirm
no glitch/reset of the pattern.

## After step 4
5. MIX knob -> native 0..6 scrub range + soft drag-only detents (widget).
6. reseed-on-roll -> reset.
7. dice undo -> (counter, slew) scalar (amended: slew included per constant-slew reversibility).
