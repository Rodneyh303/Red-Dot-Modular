# Test updates needed — PPQN pulse-grid gate timing

The gate-close mechanism moved from tick()/gateSecRemain to tickPulse()/
gatePulseRemain. holdRemain (step-unit DECISION counter) is UNCHANGED, so all
decision-logic tests (MidNote/canRest/hadTail, note-spanning, varyNoteIndex,
poly ownership) are unaffected. Only gate-CLOSE assertions need updating.

Key behavioural change to encode in tests:
  - tick()      now ONLY decrements holdRemain (decisions) + LED timers. It does
                NOT close the gate anymore.
  - tickPulse() is the sole gate-close. Call it pulsesPer16th (default 6 at 24 PPQN)
                times per step. gateHeld drops when gatePulseRemain hits 0.
  - A note of N steps closes after N*pulsesPer16th tickPulse() calls (e.g. a 1-step
    1/16 note: 6 tickPulse() calls at 24 PPQN).
  - Set g.pulsesPer16th before arming, or pass it via tick(p16). Default is 6.

## test_GateState.cpp — UPDATE gate-close assertions
Examples of the pattern that breaks and the fix:
  OLD (assumed tick closes gate):
      g.triggerNote(...);           // 1/16 = 1 step
      g.tick(); g.tick();           // expected hold to expire + gate to drop
      EXPECT(!g.gateHeld);          // FAILS NOW — tick() no longer closes
  NEW:
      g.triggerNote(...);
      g.tick();                     // decision: holdRemain 1→0
      for (int i=0;i<6;++i) g.tickPulse();   // gate closes after 6 pulses (24 PPQN)
      EXPECT(!g.gateHeld);
  General rule: replace "tick() until gate drops" with "tick() for decisions +
  tickPulse()*N for the gate." Audit every EXPECT(!g.gateHeld) / EXPECT(g.gateHeld)
  that followed tick() calls (lines ~196-224, 247-249, 273 region).
  process()-returns-voltage tests (229+) mostly hold (process() still returns
  10/0 by gateHeld + the 1ms pulse dip), but any that relied on process() itself
  closing the gate via the seconds countdown must close via tickPulse() first.

## test_fractional_tail.cpp — REWRITE (replicates the deleted model)
This file has its OWN mini-struct copying the old seconds model:
    float holdRemain, gateSecRemain, curStepSec; armGate=d*curStepSec; process-=dt...
    midNote = holdRemain>=1 || gateSecRemain>0
The whole premise (sub-step tail closed by a seconds timer) is GONE. Rewrite the
replica to the pulse model:
    int holdPulses; armGate(d){ holdPulses = round(d*p16); }
    tickPulse(){ if(holdPulses>0){holdPulses--; if(0) gateHeld=false;} }
    midNote = holdRemain>=1 || gatePulseRemain>0
The test's INTENT (a fractional note keeps MidNote until its tail ends; can be the
LAST note of a legato/tie but not the first/middle) is still valid — re-express it
counting pulses. e.g. 1/32 = 3 pulses: MidNote true for pulses 1-2, rest allowed
at pulse 3 boundary. Verify the legato/tie source/middle rules (the comments at
test_fractional_tail.cpp:1-30 and SequencerEngine.cpp:167-189) still hold — they're
musical rules independent of the timer, so they should.

## test_multistep.cpp — likely OK (decision-based)
Its tick() replica (line 107-122) decrements holdRemain by 1 per step — that's the
DECISION counter, unchanged. Should pass as-is. Verify the e.tick() calls compile
(bare tick() is fine: new signature tick(int p16=0)).

## test_note_length / test_var_leg_rest / test_poly_voices / test_edge_cases
Scan for: (a) any gateSecRemain/curStepSec reference (must go — deleted),
(b) any tick(someFloat) call (signature is now tick(int); a float arg like
tick(0.125f) would pass 0 via int-trunc-ish — actually float→int converts, 0.125→0,
harmless, but tick(sixteenthSec) with ss>=1 would wrongly set pulsesPer16th — AUDIT
these), (c) gate-close-after-tick assertions (add tickPulse()*N).
SAFEST: grep each test for "gateSecRemain", "curStepSec", ".tick(" with a float
literal, and "gateHeld" near tick() — fix per the patterns above.

## Suggested test helper (reduces churn + future-proofs)
Add to the test harness:
    constexpr int P16 = 6;  // pulses per 1/16 at 24 PPQN (test default)
    inline void closeGate(GateState& g, float steps){ for(int i=0;i<(int)lround(steps*P16);++i) g.tickPulse(); }
Then tests read intent-fully: triggerNote(1/16); closeGate(g,1.0f); EXPECT(!g.gateHeld);

## Build order
Build the SOURCE first (engine compiles standalone). Then fix tests. The source
change is self-contained; test failures are expected and listed above — they're
assertion/replica updates, not source bugs.
