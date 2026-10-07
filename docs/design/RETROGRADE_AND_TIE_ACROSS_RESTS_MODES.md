# Backward playhead (generative) + tie-across-rests across modes

STATUS: design notes / decisions. Release-scope.

## 1. Backward playhead is RELATIONAL-ONLY in a generative engine (Rodney)
Deterministic sequencers: backward playhead = play the recorded notes in reverse (meaningful, there's
a fixed sequence to reverse). OUR engine GENERATES as it reads — no pre-recorded sequence — so walking
the playhead backward through a probability field just reads positions in descending order, which for
generated draws is ANOTHER walk, not an audible "reverse of" the forward walk. **So backward playhead
buys little in ISOLATION.**
It buys something RELATIONALLY — in reference to other FORWARD playheads:
- A backward voice reads the SAME correlated field in opposite position-order, giving CONTRARY MOTION /
  convergence-divergence vs the forward voices (retrograde-canon-ish). Meaningful only because of the
  forward reference.
Implications:
- Value lives at the PER-VOICE direction level (voice direction, the addressing system) — contrary
  motion against forward voices. NOT at the global clock/gate level (global-everything-backward just
  gives a different global walk, no internal relationship change).
- **Phase mode:** backward is INHERENT to the drive model (position is the input; backward = drive the
  phase back). Native, not an added mode.
- **Clock mode backward:** easy if wanted (reuse phase's backward-advance + PPQN quantise) — but
  generatively buys little absolutely; only a reference frame for EXTERNAL forward modules.
- **Gate mode backward (if ever):** NOT reverse-time — time still forward, gates still rise/fall
  forward; just DECREMENT playhead on rising edge, stop on falling (unchanged), MIRROR the subgate
  boundary logic (reflect the straddle/coincidence/ghost rules). Bounded (forward-time model + legato
  untouched; only playhead-walk-dir + subgate-mirror change). Composes as the playhead sign in the
  three-factor direction product (playhead x voice x lane).
**Decision lean:** global backward playhead in clock/gate not pursued (relational value already met by
per-voice voice direction; phase covers driven-backward natively). Per-voice backward direction (the
addressing system) is where retrograde earns its place.

## 2. tie-across-rests should extend to clock/phase — CORRECTED
All THREE modes (gate, clock, phase) have BOTH rest types:
1. **Generated rests** — a step/event that is a rest (`generatedRestBeatsLegato` governs rolling these).
2. **Gap / incoming rests** — a gap before the next note (`tieAcrossRests` governs bridging these).

So clock/phase are NOT missing a rest type. The clock/phase **RESIDUAL GAP** (the leftover when an odd,
non-1/16 note is quantised to the 1/16 grid) is the **EQUIVALENT of gate mode's INCOMING REST = the gap
between gates** (Rodney). It is the same gap-type rest, and `tieAcrossRests` governs it identically.
(The 1/16 note-length restriction in clock/phase is a SEPARATE thing — it's merely the mechanism that
PRODUCES the residual gap; it doesn't change the rest model.)

**So both toggles apply to all three modes.** Clock and phase currently just DON'T ALLOW tie-across-gap
— the gap-bridge behaviour isn't enabled there, even though they already HAVE both gap types. The
extension is therefore close to "enable tie-across-rests (the gap-bridge logic) in clock/phase" — they
are already structured for it; they simply don't act on the gap-bridge toggle yet.

- `tieAcrossRests` → enable in clock/phase; bridges the residual/incoming gap (= gate's between-gates
  gap) identically.
- `generatedRestBeatsLegato` → ALSO applies (clock/phase DO have generated rests) — enable consistently.

Verify the tie-across-rests implementation is at the legato/rest-decision level (mode-agnostic per the
gate-mode discipline) so enabling it for clock/phase's gaps is clean.

### DEFAULT (Rodney) — tie only DIRECTLY-CONNECTED; residual gap is a REAL gap, not auto-tied
Correcting an over-reach: the residual gap is NOT "incidental so bridge by default". It is a GENUINE
gap (the odd note ended before the next grid position). Default behaviour parallels gate mode exactly:
- **Gate mode default:** auto-tie across gates DIRECTLY CONNECTED — separated only by the MINIMUM
  rise/fall that can distinguish two gates (the <=1-sample abutment / overlap case). Real gaps are NOT
  auto-tied; `tieAcrossRests` is the explicit opt-in for those.
- **Clock/phase default:** auto-tie across **1/16-MULTIPLE** notes (grid-adjacent, directly connected
  — the equivalent of gate's minimum-separation directly-connected case). The RESIDUAL GAP from a
  triplet/odd note is a real separation → NOT tied by default; `tieAcrossRests` is the opt-in to bridge
  it (and `generatedRestBeatsLegato` for generated rests).

Same principle across all modes: **auto-tie only the directly-connected / minimum-separation case;
anything with a real gap (residual OR structural/generated) requires the explicit tie-across-rest
toggle.** The residual gap is treated like any other real gap — default respect, opt-in bridge.
