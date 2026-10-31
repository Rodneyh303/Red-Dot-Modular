# PPQN 48/96 — nominally supported, NOT actually safe (the sequencer double-speeds) (Rodney)

STATUS: a real latent issue, SEPARATE from dotted notes (surfaced while scoping dotted-1/32). Documented so
nobody builds on the false "we support 48/96" belief. Not scheduled; a genuine clock feature if pursued.

## THE SYMPTOM (Rodney, Rack)
At 48 PPQN the sequencer runs TWICE AS FAST (96 would be 4×). It does NOT resolve finer at the same tempo —
it reinterprets the incoming clock as a higher rate, so tempo multiplies.

## THE CAUSE
ClockEngine.hpp:26-29: pulsesPer16th(ppqn) = ppqn/4 (24→6, 48→12, 96→24 internal pulses per 1/16). The
comment states the assumption: "an external clock is ASSUMED to arrive at exactly ppqn PPQN (one incoming
pulse = one grid pulse) — a rate mismatch just scales tempo." So at ppqnSetting=48 the engine EXPECTS the
external clock to deliver 48 pulses/quarter; a standard 24 PPQN clock then makes every incoming pulse worth
2× the intended time → the sequencer advances at double speed. 48/96 only behave correctly if fed a
MATCHING faster external clock — i.e. they REQUIRE a matching clock, contradicting "allow but don't require".

## THE CONTRADICTORY / STALE COMMENTS (reconcile)
- Monsoon.cpp:724: "ppqnSetting raw values: 1, 4, 24" — the ACTUAL supported set.
- SequencerEngine.cpp:742: "PPQN is now always 24/48/96 — all of which resolve every note value" — WRONG on
  two counts: (a) 48/96 are never SET (ppqnSetting defaults to 24, ppqnMask hardcoded to 4=PPQN24 at :746,
  so note-value gating always behaves as 24 regardless), and (b) if they WERE set they'd double/quad the
  tempo (above). The :742 comment is aspirational/stale; :724 is the truth. Reconcile them (fix the :742
  comment; the engine effectively runs at 24).

## CONSEQUENCE FOR NOTE VALUES (and why dotted stops at 1/16)
Every current note value + the 4 added dotted (1/2. 1/4. 1/8. 1/16.) resolve to integer pulses at 24 PPQN,
so they're all legal at the real baseline. DOTTED-1/32 needs 48 PPQN (4.5 pulses at 24, fractional; 9 at
48) — but 48 isn't actually safe, so dotted-1/32 is EXCLUDED. It is NOT a dotted-table addition; it depends
on 48 PPQN being real.

## IF 48/96 ARE EVER WANTED FOR REAL — BOUNDED, not colossal (Rodney)
The scope is MEDIUM, not high-blast-radius, BECAUSE the timing is already funnelled through ONE pulse
source. Every gate/subgate/ratchet/step consumer ticks off ClockEngine::pulsesPer16th(ppqnSetting) via
gs.tick (SequencerEngine.cpp:1144-1153) — they are DEFINED in terms of pulses, not hardcoded step counts.
So subgates / ratchets / step-advance / Cantonment gate timing do NOT each need rework: fix the pulse rate
and they are correct by construction. The "one authority" discipline (one pulse rate, everything downstream
pulse-defined) is exactly what keeps this bounded.
THE ACTUAL CHANGE (small/medium, localized):
1. CLOCKENGINE external-clock multiplier (the real work, ONE file): pulsesPer16th assumes external =
   ppqnSetting (:26-29). Fix = generate N internal pulses per incoming pulse (interpolate between external
   edges / PLL), so 48/96 = finer grid at the SAME tempo from a 24 PPQN external clock. OR decouple
   ppqnSetting (internal subdivision) from the external rate (tempo). Either is contained in ClockEngine.
2. Un-hardcode ppqnMask (:746) to reflect the real ppqnSetting — trivial.
3. Add 48/96 bits to allowedPPQN (table edit) — trivial.
THE REAL COST IS TESTING, NOT CODE: the consumers don't change, but the finer regime has NEVER been
exercised, so VERIFY the whole timing layer BEHAVES correctly at 48/96 (step/subgate/ratchet/ghost,
step-advance, Cantonment) — testing breadth, not code breadth. Bounded, focused feature; NOT a note-value
tweak, but NOT colossal. (Corrects an earlier 'high blast radius' overstatement — the single-pulse-source
architecture makes it medium.)

## RECOMMENDATION
- Ship dotted at 4 values (to 1/16) — correct at 24, no clock change.
- Leave 48/96 as-is (dormant/unsafe) but FIX the stale :742 comment so the truth is recorded.
- Treat "real 48/96 PPQN (finer at same tempo)" as a separate, deliberately-scoped, fully-tested feature if
  ever wanted — not something to back into via dotted-1/32 or a note-value addition.

════════════════════════════════════════════════════════════════════════════════
## DECISION (Rodney) — require 24 PPQN; 48/96 de-prioritized to near-zero
Context: the note-value set is FIXED at 13 entries (1/1 … 1/32 incl all dotted + 1/16T), ALL of which
resolve cleanly at 24 PPQN. So 48/96 is no longer needed to UNLOCK any note value — its only remaining use
would be sub-pulse timing precision, which isn't worth the work.

DECISION:
- REQUIRE 24 PPQN. 24 is the more standard clock rate; the instrument expects a 24 PPQN clock.
- NO clock divider (Rodney) — not adding an input divider; just expect 24.
- 48/96 stay nominal/unsafe (they double/quad-speed — the clock assumes external rate = ppqnSetting). NOT
  fixed; LOW PRIORITY, near-zero (nothing depends on it now the note values are capped).
- Fix the stale SequencerEngine:742 comment ("always 24/48/96 all resolve") so the record is honest: the
  instrument runs at 24; 48/96 are not supported.
- If finer timing/resolution is EVER wanted it's the separate ClockEngine-multiplier feature above — but
  there is no pressure for it; skip indefinitely unless a concrete need appears.
NET: require 24, document it, don't build 48/96 or a divider. The capped note-value set made the whole
48/96 thread unnecessary.
