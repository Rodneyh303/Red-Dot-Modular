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

## 2. tie-across-rests should extend to clock/phase — but the rest MODEL differs
Gate mode built the two-toggle rest model: `generatedRestBeatsLegato` (rolled/GENERATED rests) +
`tieAcrossRests` (bridge a structural GAP). Clock/phase have a DIFFERENT rest model:
- **Clock & phase: notes restricted to integer multiples of 1/16.** An odd (non-1/16) note quantised
  into the grid leaves a RESIDUAL GAP (leftover to the next 1/16 boundary), treated as an INCOMING
  rest (belongs to the next note's arrival) — NOT a separately-generated rest step.
- So "rest" means structurally different things: gate = a generated rest EVENT; clock/phase = a
  RESIDUAL GAP from 1/16-quantising odd durations, framed as incoming.
Consequence for extending tie-across-rests:
- **`generatedRestBeatsLegato` has NO referent in clock/phase** (no generated rests) → stays gate-only.
- **`tieAcrossRests` DOES extend** → it governs bridging the RESIDUAL-GAP incoming rest: when an odd
  note leaves a residual gap before the next note, does the legato SUSTAIN through the gap into the next
  note (tie) or does the gap break it? Well-defined, and SIMPLER than gate (only one gap type — the
  residual/incoming — no generated-vs-structural split).
**So the extension is:** enable `tieAcrossRests` in clock/phase governing the residual-gap bridge; keep
`generatedRestBeatsLegato` gate-only. Not "ungate the toggle" — it's the same structural-gap-bridge
toggle applied to clock/phase's residual-gap rests (their only rest type).
Verify: confirm the tie-across-rests implementation is at the legato/rest-decision level (mode-agnostic
per the gate-mode mode-agnostic discipline) so applying it to residual gaps is clean.
