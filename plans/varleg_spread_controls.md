# VAR/LEG — remaining work: SPREAD CONTROLS (poly data now works, only controls missing)

State (Rodney): after hours of work, poly VAR/LEG now show proper PER-VOICE data (CC's f9c4189 added
slewedPolyVariation/Legato -> polyRandom). The GENERATION is done. **All that's missing is the SPREAD
CONTROLS** — the SPR jack/attenuverter/base-spread knob on the VAR/LEG lanes, plus routing them
through the manager's spread so they actually apply. Do NOT touch the working poly-draw code; this is
additive controls only.

REST is the template for every step — VAR/LEG are the identical chain at editor lanes 5,6.

## Remaining steps (each its own commit, test between)
1. **Manager spread routing** — MonsoonExpanderManager.cpp:314-325 still says "No Macro blend and no
   spread ... the probability array stays mono". That premise is now FALSE (poly draws exist). Remove
   the VAR/LEG spread special-case; route VAR/LEG through `combineSpread` exactly like REST (owner
   picks East per-voice interp vs Macro base; negative-spread inversion). Delete the stale comment.
2. **SpreadInterp** — confirm applyMono/applyPoly cover lanes 5,6 (N_SPREAD_LANES is 7; verify the
   indexing reaches VAR/LEG, not just 0..4).
3. **Panel SPR control** — StraitsEastSandsVisual.cpp:352 is the "LEN/OFF/ROT only — no SPR" block.
   - gen_east_clean.py hardcodes POLY_LANES=5 and emits varlegcv rows cols 0..2 only — make it 7
     lanes and emit the 4th (SPR) column: anchors param_spr_5 / param_spr_6 and the SPR CV jack
     input_varlegcv_<vl>_3.
   - widget: extend the varlegcv loop c<3 -> c<4 (SPR col); bind the SPR CV jack + depth atten +
     base-spread knob (SPREAD_V / SPREAD_L param ids) like the other lanes.
4. **VERIFY SPREAD APPLIES, not just appears** — sweep VAR spread on a poly patch; per-voice
   variation correlation must actually change (voices converge/diverge), marginals preserved. A
   control that renders but does nothing is NOT done (the recurring failure mode).

## Known pre-existing (NOT caused by controls work)
test_per_voice_articulation (768 fail) + test_subgate (1 fail) are RED on f9c4189 already — fallout
from the poly-draw change. Separate from the spread-controls task; triage separately (the 768 is the
test asserting old mono-mirror behaviour that the new per-voice draws deliberately changed — likely
the TEST needs updating to the new per-voice model, not the code).
