# TODO — verify Lantern display correctness in GATE and PHASE modes

Rodney (post gate-marathon): during the gate work I watched the VCV SCOPE almost exclusively, so
Lantern's own display may NOT have always been correct in gate mode — never confirmed. Needs checking.
Also check PHASE mode.

## Why it's suspect
Lantern visualises engine state (playhead / step / lane activity). The gate + subgate + ghost work
changed WHEN and HOW the playhead advances:
- playhead now advances on main-gate rise, ratchet onset, AND ghost onset (edge-driven), not just a
  clock step — Lantern may assume clock-step advance.
- subgate/ghost events, tie-across-gap, and the mode-agnostic path all move or re-time what Lantern
  should show.
- PHASE mode drives the playhead from a position ramp (PPQN-quantised), a different advance source again.
Any of these could leave Lantern reading a stale/clock-era signal or a different field than the audio
path uses.

## What to check
1. GATE mode: does Lantern's playhead/step indication match the actual sounding step — including when
   ratchets/ghosts advance it, and across tie-across-gap? Compare against the gate OUTPUT on a scope.
2. PHASE mode: does Lantern track the phase-driven playhead correctly (PPQN steps), not a free-running
   or clock-assumed position?
3. Does Lantern read PUBLISHED engine state (not live buffers)? Same display/engine race class that
   bit the Sands spread work — if Lantern reads live state it can tear. (See
   DISPLAY_STORE_ENGINE_SEPARATION.md.)
4. Per-voice: in gate mode with per-voice rest/legato, does Lantern show the right per-voice activity?

## Priority
Not urgent (cosmetic/diagnostic, not audio), but do it before the mode collapse touches mode routing
again — a correct Lantern is a useful check during that work. Add to the general test/verify pass.
