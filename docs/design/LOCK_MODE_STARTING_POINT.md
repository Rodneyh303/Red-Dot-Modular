# Lock mode -- verified starting point (before building)

## Current lock representation
- engine.locked is a SINGLE GLOBAL BOOL (PatternEngine.hpp:44), toggled at Monsoon.cpp:850,
  persisted in JSON (MonsoonPersistenceManager.cpp:277). Aliased as Monsoon::locked (Monsoon.hpp:776).
- SequencerEngine has its own separate bool locked (SequencerEngine.hpp:397) -- check if related
  or independent before touching.

## KEY FINDING: the bool is fine; the SCATTER of checks is the problem
Lock mode does NOT need finer-grained lock STATE. A single binary lock is compatible with the
LATCH/LIVE/QUEUE model -- each control's CATEGORY (not the lock state) decides its behaviour.
The actual problem is that lock's EFFECTS are implemented as ~15 scattered `if (!locked)` checks,
each independently (and implicitly) encoding a category decision. The LATCH/LIVE/QUEUE model is
about REPLACING those scattered ad-hoc checks with one coherent place that knows each control's
category and applies it consistently.

## First step of lock mode = AUDIT, not build
The doc says lock mode "inverts current behaviour" -- you cannot invert what you have not
catalogued. So step 1 is: enumerate every current lock-gate site and classify what category it
CURRENTLY implements, then compare against the LOCK_SEMANTICS.md §9 table (LATCH/LIVE/QUEUE, V/X).
Some checks may already be correct; some the wrong category; some MISSING (a control that should
respond to lock but doesn't).

## Enumerated lock-gate sites (the audit worklist)
- Monsoon.cpp:319                       -- (regenerate rhythm pattern skipped when locked)
- Monsoon.cpp:~898-900                  -- scale faders NON-DESTRUCTIVE under lock (comment only)
- MonsoonExpanderManager.cpp:109        -- CA transform apply: (boundary && !locked) || unlock
                                           (the QUEUE + unlock-flush case; step b lives here)
- MonsoonExpanderManager.cpp:385        -- (delegation? classify)
- MonsoonExpanderManager.cpp:406        -- (direction? classify)
- MonsoonExpanderManager.cpp:434        -- (classify)
- MonsoonExpanderManager.cpp:463        -- (classify)
- MonsoonExpanderManager.cpp:548        -- (classify)
- MonsoonModeController.cpp:63          -- (classify)
- MonsoonSandsManager.cpp:325           -- (classify)
- MonsoonSandsManager.cpp:452           -- spread only (V1 LOR runs UNDER lock -- note asymmetry)
- MonsoonSandsManager.cpp:547           -- (classify)
- MonsoonScaleManager.cpp:~42           -- scale fader not touched under lock (comment)
- PatternEngine.cpp:199                 -- if (in.locked) return;  (freeze)
- PatternEngine.cpp:328                 -- if (in.locked) return;  (freeze)
- PatternEngine.cpp:396                 -- if (in.locked) return;  freeze everything: seeds/RNG/patterns

Note the ASYMMETRY already present: some things run UNDER lock (V1 LOR, transpose per doc),
others freeze (seeds/RNG/patterns). That asymmetry IS the LATCH vs LIVE distinction, currently
hand-coded per-site. The audit turns it into a table-driven decision.

## Recommended order for lock mode (NOT manager cleanup first)
1. AUDIT: classify every site above against LOCK_SEMANTICS.md §9. Output: a per-site category
   (LATCH/LIVE/QUEUE) + "correct / wrong / missing" verdict.
2. Build the lock model: one place that maps control -> category -> behaviour, and first-class
   boundary + unlock EVENTS in the engine (serving the semantics, not relocating old conditions).
   The expander-manager cleanup (step b) happens HERE as a consequence -- the manager subscribes
   to engine events instead of shadowing caV2PrevStep_/caV2PrevLocked_.
3. MIGRATE controls to categories incrementally, one group at a time, build-verified each step
   (same discipline as the undo branch). CA scatter (already a QUEUE control) is a natural first
   proving case -- which is why its unlock-flush is already entangled with step b.

## Why NOT manager-cleanup-first
Step b (route boundary/unlock trigger from engine) is DEFINED BY lock semantics -- the unlock-flush
clause only has correct behaviour once unlock is defined. Moving the trigger first hard-codes the
current accidental unlock behaviour into a new location before deciding if it's right. The cleanup
is the PAYOFF of building the model, not the setup.

## Caveat
Lock mode touches the LIVE SIGNAL PATH (not just history like undo), and inverts current behaviour
in places. Bigger + riskier than undo. Own branch, own careful proving sequence.
