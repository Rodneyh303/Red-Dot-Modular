# Reversible Mode — Design Decisions & Rationale

Mode E (phase-ramp clock on CV1) lets the sequencer be driven forward **and backward**
by an external phase signal. Making the stochastic pattern *reproduce* when played
backward is the hard part. This document records how we arrived at the current design,
including a substantial approach we built and then deliberately abandoned — so the
reasoning isn't lost and isn't re-litigated by accident.

---

## 1. The core problem

The pattern is drawn from a PRNG. Playing forward, each phrase boundary may draw a new
pattern. Playing **backward**, we want the *previous* pattern to come back exactly. A
sequential PRNG (Xoroshiro) can't do this — it only goes forward and keeps no history.

**Foundational fix (kept): counter-addressable Philox.** We migrated pattern draws from
sequential Xoroshiro to **Philox4x32**, a counter-based PRNG where draw N is a pure
function `philox(key, N)`. 32-bit is sufficient: a 24-bit-mantissa float is exact float
precision, which is all the probability lanes need. Two independent streams — RHYTHM
(owns rhythm, variation, legato, accent) and MELODY (owns melody, octave) — each with
its own key, signed draw-index, and a fixed per-draw CHUNK of stream positions
(DRAW_CHUNK = 1024). `unit() = philox.atUniform(index*CHUNK + cursor++)`.

This makes any draw index reproducible in isolation. The remaining question was: *how do
we know which index to regenerate when crossing a boundary backward?*

---

## 2. The abandoned approach: the audition "tape" + BoundaryDice

We first tried to make the EXISTING feature set fully reversible. That set includes:
- **Auditioning (trial dice):** draw candidate B's against a fixed A without committing,
  then promote one. This decouples the committed A-trajectory from the raw draw count.
- **Reseed-on-roll, live (realtime) mode, full-entropy reseeds:** some of these draw
  from non-counter entropy, so they're inherently non-reproducible.

To reverse through that, we built:
- A per-stream **audition-count tape**: a preallocated power-of-two ring
  (`unique_ptr<uint16_t[]>`, 1<<20 entries = 4 MB total), written forward, read backward,
  recording how many auditions preceded each committed roll so reverse could step the
  draw-counter back by `(auditions+1)` per roll. Tape-machine semantics: a forward roll
  while behind the head truncates (record-over-rewind fork). Sliding window, O(1), no
  audio-thread allocation.
- A per-stream, per-**boundary** `BoundaryDice` record (Reuse / Roll / NonReproducible),
  because most boundaries in dice mode are *reuse* (no draw) and reverse must skip
  exactly those, while full-entropy boundaries can't be reconstructed at all.

This **worked** (the tape and boundary logic were unit-verified) but it was a large,
intricate apparatus whose entire purpose was to make auditioning + entropy reversible.

### Why we abandoned it

Auditioning is fundamentally at odds with positional reversibility: a jump/scrub
traverses *position*, but an audition is a *user action* that isn't a position. Reverse
can't ask the user to re-perform auditions, so it must replay a recorded history — and
once the user can audition *differently* on the way back, or fork the timeline, the
bookkeeping explodes (forks, floors, non-reproducible clamps, live-entropy logging…).

The realization: **we were adding complexity to preserve features that fight
reversibility.** Cutting that knot is better than managing it.

---

## 3. The chosen design: reversibility as a per-stream MODE

Reversibility and auditioning are made **mutually exclusive, chosen up front, per
stream** (rhythm and melody independently).

### NORMAL mode (default)
All existing features available: auditions, reseed-on-roll, live trial source, etc. No
backward draw-tracking. When the phase runs backward, a Normal stream **keeps rolling**
(draw index still steps +1) — i.e. reverse just produces new forward-style draws. (The
*playhead* still traverses the phrase backward within the current draw — that's
independent and always works. One stream may be reversible while the other isn't.)

### REVERSIBLE mode (opt-in, per stream)
Pure stochastic dice. The draw index is a **signed counter**:
- forward armed roll → index **+1**
- reverse armed roll → index **−1**
- **no floor, no ceiling.**

Reverse stepping is triggered simply: **arm the dice; the index decrements at the next
phrase boundary** (an unarmed boundary is a reuse — no step, same as forward).

Reversible mode **blocks** the incompatible features for that stream: trial arming,
trial-as-live-source, and reseed-on-roll. The ONLY state is the current index. No tape,
no boundary record, no history buffer, no 4 MB ring. The entire abandoned apparatus
collapses to one integer per stream.

### Why no floor/ceiling — the key insight
Philox is a **keyed bijection over the full counter space**. A negative index is just
another valid block input, so index −1, −1000000, etc. are all perfectly valid,
reproducible draws — not an error to guard against. We assumed the counter had to be
non-negative; it does not. So reversing past the start doesn't bottom out on a special
"initial pattern" — it produces fresh-but-reproducible draws indefinitely in either
direction. Verified: a 0 → −2 → 0 round trip reproduces bit-exactly; distinct indices
give distinct, non-overlapping chunks even across the zero boundary; the
`(uint64_t)signedIndex * CHUNK` addressing is a bijection mod 2^64 and never aliases
(you'd need 2^54 draws to wrap). "Index 0" is special only by convention (where reset /
mode-entry put you), not structurally.

---

## 4. Mode-change semantics (global context-menu options)

On **entering** reversible mode for a stream, two global toggles decide the origin:

| Reseed on mode change | Reset index on mode change | Behavior on entry |
|---|---|---|
| ON (default) | (greyed — irrelevant) | Reseed key **and** zero index → clean reproducible fresh start |
| OFF | ON (default) | Keep key, zero index → index 0 of the current seed |
| OFF | OFF | Keep key **and** index → seamless "reversible from exactly here" (nonzero origin, fine) |

- **Reseed ALWAYS zeroes the index** — a coupled invariant (a fresh key meaningfully
  starts at index 0). This removes the meaningless "reseed but keep nonzero index" combo
  by construction.
- Because reseed implies reset, the "Reset index on mode change" toggle only matters when
  reseed is OFF — so it is **greyed out in the menu when reseed is on** (its name reflects
  that it's the no-reseed reset policy).
- Both default ON → default entry = reseed + zero = clean fresh start. A performer who
  wants zero disruption turns both off.

These are **global** (not per-stream): one policy applied to whichever stream is being
switched. The reversible toggle itself is per-stream.

All four settings (rhythm mode, melody mode, reseed-on-change, reset-index-on-change)
persist in the patch JSON.

---

## 5. Naming

The earlier framing ("reversible vs non-reversible") described a limitation. The honest
framing is the reverse: **Normal** is the full-featured default; **Reversible** is the
*constrained* opt-in that blocks some features to buy deterministic backward play. The
menu reads Normal / Reversible per stream.

---

## 6. What's kept vs dropped

**Kept (foundation):** counter-addressable Philox draws (signed index, per-stream
chunks); Mode E phase engine (forward/reverse/jump/within-draw playhead); the UI
direction cues (light-ring comet trail, Sands leading-edge marker).

**Dropped (the abandoned tape branch lineage — NOT merged):** the audition-count tape,
the BoundaryDice per-boundary record, fork truncation, the 4 MB ring, restoreInitialAB /
floor-clamp, the non-reproducible-entropy logging. Reversible mode needs none of it.

**Blocked in reversible mode only (still available in Normal):** trial/audition arming,
trial-as-live-source, reseed-on-roll.

---

## 7. Verification status

Standalone unit tests (Rack-independent) pass for: signed-index forward/reverse
reproducibility, negative-index round trip (0 → −2 → 0 bit-exact), chunk distinctness
across the zero boundary, and the underlying Philox conformance (KAT + std::philox4x32
anchor). The integration is brace-audited but UNBUILT in this environment (no Rack SDK) —
audible behavior to be confirmed on a Windows/MSYS2 build.
