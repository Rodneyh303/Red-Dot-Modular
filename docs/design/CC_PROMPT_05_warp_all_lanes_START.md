# CC PROMPT 05 — warp ALL lanes/voices through ONE accessor (kill the mono/lane-special bug class)

CONTEXT: the warp draw bug (CC_PROMPT_04_FIX_warp_draw) was a mono/voice-0-special residue — warp
wired for voice 0, not poly. Rodney: end the "voice-is-special / lane-is-special" bug hunts for
good by making the plumbing UNIFORM — every lane, every voice, reads its uniform through ONE
accessor (warpRow), no per-lane gate branch, no mono/poly branch. Do this AFTER / together with
the draw-bug fix (they touch the same sites — coordinate so they don't conflict).

## THE PRINCIPLE (this is the whole point — structural, not behavioural)
There must be EXACTLY ONE way any consumer gets a step's uniform: warpRow(v, lane). No
`if (lane==MELODY||lane==OCTAVE)` gate, no `displayedVoice_==0 ? mono : poly` branch, no direct
polyRandom/polyRandomSrc/random_ reads in any CONSUMER. One path ⇒ "lane is special / voice is
special" divergence becomes IMPOSSIBLE BY CONSTRUCTION — there is no second path to drift.
(This is the same discipline that ended the convention war: one accessor, contract-tested.)

This is SAFE and mostly BEHAVIOUR-PRESERVING because:
- warpRow(v,lane) = warpBank_ ? warped_[v][lane] : random_[v][lane] (PatternEngine.hpp:181-185).
- At the DEFAULT (IdentityWarp + w=0, which every lane/voice starts at), warped_ == random_
  BIT-EXACTLY (proven: test_warp_f_bank S7 / test_warp_mirror no-op@w0). So a boolean/qmix lane
  routed through warpRow returns the SAME uniform it does today, until someone sets a curve+w>0.
- warpRow's own comment says it's a "one-token swap" for polyRandom call sites with no read-site
  branching. It was BUILT for exactly this.

## WHAT CHANGES vs WHAT DOESN'T — be precise
- CHANGES: the PLUMBING. Every consumer reads warpRow(v,lane). The per-lane GATE (currently in
  finalRandomByStrand :511 "MEL||OCT only", and the raw reads in executeStep/executePolyVoice)
  is REMOVED as a branch.
- DOES NOT CHANGE: the EFFECT, by default. Boolean/qmix lanes default to IdentityWarp+w=0 ⇒ inert.
  The per-lane distinction becomes "which lanes DEFAULT to a non-identity curve" (none, for now) —
  a DEFAULT, not a code branch. So no audible change on rest/accent/qmix/variation/legato today.
- NOTE ON REDUNDANCY (why this isn't "new behaviour on booleans"): on a THRESHOLD lane static warp
  is redundant with the threshold knob ({F^-1(U)>t} ≡ {U>F(t)}, -18b §3). So even if a user sets a
  curve on rest, static warp just shifts the effective threshold — not new, not harmful. The real
  boolean-lane payoff is MODULATION response, a later feature. This prompt is PLUMBING only; it does
  not enable boolean warp as a feature, it just removes the branch so the path is uniform.

## THE WORK (mechanical — ~26 one-token swaps, verified)
Route EVERY consumer uniform-read through warpRow(v, lane). Swap at the READ site:
    polyRandom(v, LANE)[idx]      → warpRow(v, LANE)[idx]
    polyRandomSrc(v, LANE)[idx]   → warpRow(v, LANE)[idx]   (keep the refreshPolyRandomCell that
                                     writes random_ first — warpRow reads the mirror built from it)
    random_[v][lane][...]  (in consumers) → warpRow(v, lane)[...]
SITES (grep to confirm the full set — SequencerEngine.cpp + PatternEngine.cpp):
- MONO (executeStep feeders, ~:753-757, :803, :850-854, :950-953): polyRandom(0, PL_*) → warpRow(0, PL_*)
- POLY (executePolyVoice, :1080 r_rest, :1095 r_vary, :1138 r_qmix, :1148 mel/oct): polyRandomSrc(voiceIdx, PL_*) → warpRow(voiceIdx, PL_*)
- DISPLAY/DRAW (PatternEngine.cpp :787 redrawMelody, :886-889, the qmix/rhythm redraw twins):
  slewed*[0] / polyRandom → warpRow(voice, ·)
- finalRandomByStrand (PatternEngine.hpp:511): REMOVE the MEL||OCT gate → return warpRow-equivalent
  (warped_) for ALL strands. (At identity defaults this is byte-identical to today for non-mel/oct.)
CRUCIAL: after this, there should be NO polyRandom/polyRandomSrc/random_ read in any CONSUMER
(threshold, pickSemitone, pitch, CV out, display). Those accessors remain ONLY as the WRITE target
(refreshPolyRandomCell writes random_; the mirror reads it). Grep to prove: consumer reads = 0
outside warpRow.

## MIRROR COVERAGE — the mirror must now be fresh for ALL lanes
Today the rebuild hooks may only cover mel/oct (the only warped lanes). Now ALL lanes read the
mirror, so EVERY lane's mirror cells must be rebuilt when random_ changes. Confirm/extend the 03
rebuild hooks (recomputeEffective*, refreshPolyRandomCell, manager spread passes) to rebuild ALL 7
lanes' cells, not just mel/oct. Cost is fine (full 1792 rebuild = 0.064ms). The simplest correct
rule: every random_ writer rebuilds the cells it wrote for ALL lanes. (At identity this is a
same-value copy — cheap and harmless — but it keeps the mirror coherent for when a curve IS set.)

## THE CONTRACT TEST — the thing that makes the whole bug CLASS impossible to ship
test/test_warp_all_lanes.cpp (or extend test_warp_mirror). Assert, for EVERY lane (0..6) × EVERY
voice (0..15):
  1. DEFAULT NO-OP: IdentityWarp + w=0 ⇒ the consumer uniform == raw random_ BYTE-IDENTICAL.
     (proves routing-through-warpRow changed nothing at defaults, all lanes, all voices.)
  2. WARP REACHES: set a non-identity WarpF + w>0 on (voice, lane) ⇒ the consumer uniform for THAT
     (voice, lane) == the mirror value (≠ raw); and OTHER (voice, lane) cells UNCHANGED.
  3. NO RAW LEAK: (static/structural) assert via grep-in-test or a code check that no consumer path
     returns a raw-bank value when a bank is wired — i.e. warpRow is the sole uniform source.
Assert 1 for ALL 112 (voice,lane) cells is the key: it proves uniform plumbing with zero behaviour
change at defaults. RED on any site still reading raw; GREEN when all route through warpRow.

## DONE =
- Every consumer reads warpRow(v,lane); per-lane gate + mono/poly branches REMOVED from read sites.
- polyRandom/polyRandomSrc/random_ survive ONLY as write targets, not consumer reads.
- Mirror rebuild hooks cover all 7 lanes.
- test_warp_all_lanes: default-no-op for all 112 cells + warp-reaches per cell + no-raw-leak; RED→GREEN.
- bash test/run_all.sh green; plugin builds; audit green.
- PR body: confirm byte-identical output at defaults (the no-behaviour-change proof), list the swapped
  sites, "needs Rack feel-test: output unchanged with no curves set; warp reaches every voice when set."

## NOT THIS PROMPT
- Enabling boolean-lane warp as a FEATURE (modulation response) — plumbing only; defaults inert.
- The popup z-order fix (separate prompt).
- Clamp / atoms (deferred).
