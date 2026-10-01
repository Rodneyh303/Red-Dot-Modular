# Monsoon rhythm-behaviour context-menu toggles — reference

A quick reference for two of the toggles in the Monsoon right-click → rhythm-behaviour menu.
Both are persisted to the patch JSON
([`MonsoonPersistenceManager.cpp`](../../src/dsp/managers/MonsoonPersistenceManager.cpp:27)).

---

## "Per-voice articulation (East VARIATION/LEGATO)" — `perVoiceArticulation` (default OFF)

Member: [`SequencerEngine::perVoiceArticulation`](../../src/dsp/engines/SequencerEngine.hpp:331).
Driven in [`executePolyVoice`](../../src/dsp/engines/SequencerEngine.cpp:902). Part of the
EAST_EXTRA_LANES stage-2 poly work (per-voice articulation, clamped to the mono event grid).

- **OFF (default):** every poly voice uses the **mono** note-length and legato decisions exactly —
  all voices articulate in lockstep with the mono voice (rest/legato/tie shape is shared). This is
  the legacy behaviour.
- **ON:** each poly voice gets its **own** articulation, authored via per-voice **VARIATION** and
  **LEGATO** LOR lanes on the East Straits Sands expander ("Local East"):
  - each voice reads its **own** VARIATION cell → its own note-length index
    ([`nvIdxForVoice`](../../src/dsp/engines/SequencerEngine.cpp:1272)), **clamped** so a voice may
    release early but never hold past mono's next event (articulation is **subtractive**);
  - each voice rolls its **own** legato/slur commitment (Rule 2,
    [`SequencerEngine.cpp:994`](../../src/dsp/engines/SequencerEngine.cpp:994)) — a voice can
    independently opt out of a slur chain and re-articulate, or continue, independently of mono's
    chain.

**Doubly inert by default:** even when ON, the per-voice VAR/LEG LORs default to identity
(len 16, off 0, rot 0), so voices read mono's own index → bit-identical to OFF until you actually
set a Local-East VAR/LEG lane on a voice. Turn it on only once you have authored per-voice
variation/legato on the East expander.

---

## "Interrupt at phrase boundary" — `boundaryInterrupt` (default OFF)

Member: [`SequencerEngine::boundaryInterrupt`](../../src/dsp/engines/SequencerEngine.hpp:160).
Applied at the phrase boundary (the playhead wraps from the end step back to the start step — the
loop point).

- **OFF (default) = CONTINUE.** The gate/held state carries across the loop edge — a note slurring
  at the end of the phrase can tie into step 0 of the next lap. Lap 2 can therefore **differ** from
  lap 1 (a slur or held gate crosses the boundary).
- **ON = INTERRUPT.** At the wrap, the engine force-clears the held gate (`gateHeld=false`,
  `holdRemain=0`, `slurForward=false` — mono + all poly voices, including the STEP mirror) **before**
  the next step's `wasHeld` is captured. So the note at step 0 sees no held predecessor → **every
  lap is identical** (no cross-lap memory, no slur across the loop point). Use it when you want each
  loop repetition exactly reproducible rather than letting a tail bleed into the next lap.

### Which modes does it affect?

**All six modes (A, B, C, D, E, F)** — the clear lives in the two shared engine entry points that
every mode routes through:

| Mode | Engine entry | boundaryInterrupt clear site |
|------|--------------|------------------------------|
| A (clock)            | `executeModeA` | [`SequencerEngine.cpp:649`](../../src/dsp/engines/SequencerEngine.cpp:649) |
| B (gate)             | `executeModeB` | [`SequencerEngine.cpp:716`](../../src/dsp/engines/SequencerEngine.cpp:716) |
| C (gen quantiser)    | → `executeModeA` ([`ModeController.cpp:396`](../../src/dsp/managers/MonsoonModeController.cpp:396)) | (same as A) |
| D (gate quantiser)   | → `executeModeB` ([`ModeController.cpp:419`](../../src/dsp/managers/MonsoonModeController.cpp:419)) | (same as B) |
| E (phase generate)   | → `executeModeA` ([`ModeController.cpp:238`](../../src/dsp/managers/MonsoonModeController.cpp:238)) | (same as A) |
| F (phase quantise)   | → `executeModeA` ([`ModeController.cpp:238`](../../src/dsp/managers/MonsoonModeController.cpp:238), same path) | (same as A) |

So **yes — it affects gate mode (B/D) and phase mode (E/F)**, as well as clock mode (A/C). In every
mode the wrap is detected the same way (`advancePlayhead` returns `wrapped`), and the ON path
clears the held state before the next step's `wasHeld` capture, so no mode lets a slur cross the
loop point when the toggle is ON.

> Note on Mode B/D (gate modes): the held-gate STATE between rises is driven per-sample by the
> module layer (IMPL 2b, [`Monsoon.cpp`](../../src/Monsoon.cpp:916)), so after the wrap clear the
> gate re-opens on the next rise exactly as at any other onset — the boundary interrupt only
> removes the *slur/held-predecessor* memory, not the external gate's width.
