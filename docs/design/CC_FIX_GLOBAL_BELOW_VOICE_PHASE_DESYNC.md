# Probability-lane LOR commits at the wrong step after global transport < voice length

**Reported (Rodney, Rack):** fresh start, global 16 / voice 8 — every LOR modulation
(offset, length, rotation; all lanes) lands when the voice lane wraps to step 1,
as agreed. After setting the **global transport below the voice length** (e.g. 4)
for a time and restoring it to 16, the commits start landing at step **6**, another
time step **7** — global-invariant, "as if 6 became magic."

> **SUPERSEDED — read "## RESOLVED" at the foot first.** The `advanceOffset_` account
> below is a real *lever* (injecting it reproduces the step 6/7), but the same-seed A/B
> differential later showed `advanceOffset_` is a *symptom*, not the root: with static
> modulation the dip changes nothing; only with live modulation does it diverge, and the
> true cause is the cycle-mode nesting not clamping the voice/lanes/scan to the global
> window. The fix directions in this upper section are retained for the audit trail but
> are not the ones to build — the RESOLVED spec is.

## Root cause — first pass (lever, not root; see RESOLVED)

The probability-lane commit fires correctly at `voiceFramePhase_[v] == 0`. But the
voice-frame phase is `wrapMod(t0Now + advanceOffset_[v], walkPeriod(dir, L))`, so a
non-zero **`advanceOffset_[v]`** slides where phase 0 lands relative to the transport
the panel displays:

> **displayed commit step = voicePeriod − advanceOffset_[v]**

Injecting the divergence directly (`diag`, `advancePlayhead` harness):

| injected `advanceOffset_[v]` | commit lands at transport step |
|---|---|
| 0 | 0 (step 1, correct) |
| 1 | 7 |
| 2 | 6 |

→ exactly Rodney's 7 and 6.

`advanceOffset_[v]` is the per-voice advance **divergence** accumulator:
`SequencerEngine.cpp:632  advanceOffset_[i] += (dStep − dMainStep)` inside
`foldPerVoiceAdvance_`. Setting the **global length below the voice length** makes the
per-voice advance decision diverge from the main transport (the global wraps several
times per voice cycle), so `advanceOffset_` accumulates. Restoring the global length
does **not** clear it, and the voice-frame phase only re-seeds on the voice's **own**
key — `walkKeyOf(dir, len) = dir*64 + len` (`SequencerEngine.hpp`), which does **not**
include the global length. So the drift is latched: the commit is permanently offset
by the accumulated `advanceOffset_`, and only re-authoring the voice's own length or
direction (which changes its key → re-seed) would clear it.

This is NOT the bug6 commit-edge (that fires correctly at the true wrap — see
CC_FIX_BUG6_COMMIT_EDGE_FINDING.md). This is a separate, state-corruption bug upstream:
the voice-frame phase's transport alignment drifts and never re-anchors.

## What is reproduced vs still to confirm

- **Reproduced (deterministic):** the *lever* — any non-zero `advanceOffset_[v]`
  relocates the commit to step `voicePeriod − advanceOffset_`. Both of Rodney's
  numbers (6, 7) reproduce from `advanceOffset_` = 2, 1.
- **Not yet reproduced headlessly:** the *accumulation* itself — i.e. that global <
  voice drives `dStep ≠ dMainStep` in `foldPerVoiceAdvance_`. That needs the real
  per-voice advance path (`advancePlayhead` with a live `PatternInput`), which the
  minimal harness passes as `nullptr`, so `advanceOffset_` stays 0 there. One Rack log
  confirms it: print `advanceOffset_[v]` before the global dip, during global=4, and
  after restore — it should be 0 → non-zero → stay non-zero.

## Fix directions (pick after the accumulation is confirmed)

1. **Root — stop the spurious accumulation.** In `foldPerVoiceAdvance_` /
   `decideStepAdvance_`, a plain forward voice should not diverge from the main just
   because the global shrank below its length. Needs tracing why global<voice makes
   `dStep ≠ dMainStep` for a non-wandering voice.
2. **Re-anchor on the global transition.** When `cachedLength` changes, re-anchor
   `advanceOffset_[v]` for non-wandering voices (or fold a global-generation counter
   into `frameKey_` so the phase re-seeds). NB: merely re-seeding `voiceFramePhase_`
   does NOT fix it — `t0EffNow` already includes `advanceOffset_`, so the re-seed
   reproduces the same shifted phase. The accumulated `advanceOffset_` itself must be
   corrected.

Engine-only, headless-testable once the accumulation path is driven. The display half
(voice playhead vs commit step) is consistent with the engine here — the fix is the
latched `advanceOffset_`, not the panel.

---

## RESOLVED — the root cause and the agreed spec (2026-11-18)

### What the evidence actually showed
A same-seed A/B differential (`test/diag_global_dip_advoffset.cpp`, branch
`debug/global-dip-logging`), two engines fed an identical gate stream, one holding
global=16 and one dipping 16→4→16:
- With modulation **static**, A and B are byte-identical — the dip perturbs nothing,
  `advanceOffset_` included. So `advanceOffset_` is not the cause.
- With a **live** variation-lane LOR CV swinging through the window change, the
  modulated+wandering voices diverge permanently from ~2 steps after the restore and
  the gap grows; the centre (un-modulated) voice stays byte-identical A/B — **no
  cross-voice leak**. So the perturbation is confined to voices that are both wandering
  and actively modulated, and it is the window change reaching their reads that does it.

### The root cause
In `SequencerEngine::voiceFrameTickAt`, the CYCLE branch anchors the **read** to the
global phrase — `phrasePos = wrapMod(effTick−1, lenG)`, "returns to R at every global
step 1, restarts its cycle from R each phrase instead of drifting" — but hands back a
**frame phase** on the RAW congruence (`wrapMod(effTick−1, walkPeriod)`, the comment:
"the phase stays the RAW congruence"). `tickHolders` keys the probability-lane commit
edge off that raw congruence (`wrapMod(t0EffNow, walkPeriod)==0`). And the variation
scan wraps by the raw lane length (`SequencerEngine.cpp:629`, "the LOCAL window scan,
never the global clock"). So the **read is anchored to the global; the commit edge and
the scan free-run.** That is the step 6/7 drift and the scan-overrun — one root: cycle
mode does not clamp the frame/commit/scan to the global window the read already honours.

### The agreed semantics (UAS — Rodney)
Cycle mode never free-drifts: it aligns and re-aligns the voice to the global range.
There is **ONE primitive**; the two "clamps" are consequences of it, not separate steps.

**THE PRIMITIVE — the rotation anchor.** The voice lane is at its **rotation step R when
the global is at its first step**, re-evaluated **live every global phrase** ("rotation
does the aligning", `DESIGN_POSITION_UNIFICATION.md:263`). Each nested level anchors to
its parent's first step the same way — lanes to the voice, the voice to the global. That
is the whole rule. Build *this*, not a `min()` clamp; the extent and the scan bound below
fall out of it.

- **Consequence 1 — window extent (the "clamp").** Because the voice re-anchors to the
  global start each global phrase, a global phrase *shorter* than the voice never hands it
  enough steps to finish its own window before it is re-anchored — so it *visits* only
  `min(global, voice, lane)` consecutive cells **starting at the rotation anchor `R`**,
  wrapping mod the voice length (global 4 / voice 8: `R=0` → cells 0,1,2,3; `R=6` → cells
  6,7,0,1; grow global to 16 → it resumes all 8). **R is ALWAYS the first cell reached** —
  it can never be "a step the clamp skips," because rotation *defines* the window's start,
  not a fixed 0-based slice. The read law confirms it: at global step 1 (`phrasePos=0`) the
  cycle read is `(laneTickFor(vd,1,L) + R) mod L = R` (forward; reverse anchors `R` at the
  global *end* and walks back). The cells that go unreached are exactly those *outside*
  `[R, R+G)` — which by construction excludes `R`. The `min` is the **observable**, the
  anchor is the **cause**. There is no stored clamp to latch.
- **Consequence 2 — scan bound.** The variation escape scan cannot reach past where the
  anchor re-enters, so it traverses exactly that reachable window, not the raw
  variation-lane length (today's `:629` bug). The `variation = 1` limiting case is just
  this: always-advance is bounded by the re-anchor, so the probabilistic "where
  variation=1 would have gone" must be bounded identically, or the two disagree about what
  "advance" even means.

**LIVE, not precomputed** (C1 + the live-boundary refinement,
`DESIGN_POSITION_UNIFICATION.md:212–261`). Global length + offset are the STEP-rate class —
the MeloDicer-native "patterns react instantly" controls: reposition/resize immediately,
next step. The anchor boundary is evaluated **live each step** — "is my parent at
`phrasePos==0` THIS step?" against the parent's *current* length — never a countdown or a
stored cycle. So global 8→4 at step 8 → the voice re-anchors at **12 AND 16**, not 16; C2
(keep-counting) places the next boundary after a mid-phrase change. The voice position is a
live function of the global phrase position — exactly what `voiceFrameTickAt`'s cycle
*read* already is (`phrasePos = wrapMod(effTick−1, lenG)`, recomputed every step) — so
immediate reaction falls out and nothing is latched to go stale. The re-anchor is the
**global** phrase boundary (defined by the global via C2), not a self-referential voice
period, so nothing is ever undefined. Only the LOR **values** latch at the boundary (C1b);
the window geometry is live. (An implementation that precomputes a clamp period gets the
step-12 case wrong — the doc's explicit warning.)

**THE WANDER STAYS INSIDE.** `advanceOffset_` (the escape wander) lives *within* the
re-anchored window; it cannot push voice-step-zero off the global grid (Behavior 2, not
today's free drift). So a global dip leaves no residue — the voice re-anchors at the next
*live* global-phrase boundary, which moves immediately with the STEP-rate global length.

### Worked example — voice 8, a change at step 6 (cycle mode)
Start: global length 8, voice 8, rotation 0 (voice and global aligned, so voice step 6 =
global step 6). `phrasePos = wrapMod(t−1, globalLen)`; `phrasePos==0` is global step 1.

**Global LENGTH changes 8→4 at step 6.** Length is STEP-rate, so next step it takes
effect and `phrasePos` keeps counting and folds mod the new length (C2 — it does *not*
re-seed to 0):

```
 t=6  phrasePos=wrapMod(5,8)=5   global step 6   (change happens here)
 t=7  phrasePos=wrapMod(6,4)=2   step 3 of a 4-phrase  ← folded, immediate reposition
 t=8  phrasePos=wrapMod(7,4)=3   step 4 of 4
 t=9  phrasePos=wrapMod(8,4)=0   step 1  ← RE-ANCHOR (next live phrasePos==0)
 t=10 phrasePos=wrapMod(9,4)=1   step 2 …
```
- Next step (t=7) the window **resizes and repositions immediately** — the voice read
  follows the folded `phrasePos`, and the visited extent becomes `min(4,8)=4`.
- The voice re-anchors (and the probability lanes commit) at **t=9**, the next live
  `phrasePos==0` — **not** the old step-8 boundary, and not the voice's own 8. Thereafter
  it visits 4 cells from its rotation anchor `R` each global-4 phrase (here `R=0` → cells
  0–3). Grow global back to 8 and the window grows
  back and re-anchors on the live boundary — no residue, nothing was latched.

**Global OFFSET changes at step 6.** Offset is STEP-rate too, but it's a **pure
translation** — it slides the whole window against the clock and preserves every
relationship; it does *not* enter `phrasePos` (it is additive placement,
`voiceRead = wnow + globalOffset + O`). So next step the entire pattern slides to its new
ring position, every voice/lane relationship intact, and the cycle timing, the re-anchor
boundary, and the commit timing are **unchanged** — step 6 is not special, no re-phasing.

One-liner: **length changes *when/where* the phrase wraps (the re-anchor boundary moves,
live, via keep-counting); offset changes *where the pattern sits* (a translation, no timing
change).** Both next-step immediate — neither waits for the voice's own 8.

### Absolute (abs1 / MASK_DYNAMIC) mode — rulings + worked examples

Where cycle *loops* a window re-anchored to the global first step, abs1 reads the phrase
straight and **gates** a sounding region. Two rulings (Rodney, 2026-11-18):

- **Ring = the current global phrase, not the full 32-ring.** Absolute means
  absolute-*within-phrase*. Implementation: in `maskWindowContains` and the abs1 read,
  `% kMaskRing` → `% globalLen`, and `if (L >= kMaskRing)` → `if (L >= globalLen)`.
- **Extent is a consequence of the ring/gate, not a stored clamp.** Keep `L` declared as
  the user set it; the global-phrase ring bounds what sounds. Same principle as cycle.

Law (forward, ring = global phrase `G`, voice window offset `O` / length `L` / rotation `R`):
- **Sounds** at phrase steps in `[O, O+L)` — gate `maskWindowContains(O, L, step)`, offset
  and length ONLY (rotation absent — verified `SequencerEngine.hpp`).
- **Content** at a sounding step `s`: cell `(s + R − O) mod G`; at the region's first step
  (`s = O`) the content cell is exactly `R` (the `rho = (R−O)` anchor: mask-step-1 pinned
  to rotation).

| # | setup | sounds | content | the point (test seed) |
|---|---|---|---|---|
| A1 | G8 O2 L4 R0 | 2,3,4,5 | 0,1,2,3 | muted outside `[O,O+L)`; region-start content == R |
| A2 | G8 O2 L4 **R2** | **2,3,4,5** (same) | 2,3,4,5 | **rotation moves content, not the sounding set** — so a display/extent clamp can't miss intended steps |
| A4 | G4 O0 **L8** R0 | 0,1,2,3 (all) | — | `L ≥ G` ⇒ whole phrase sounds; declared L stays 8, no cached `min()` |
| A6 | G8 O0 L4 R0 | **cycle:** all 8 (loop `0,1,2,3,0,1,2,3`) vs **abs1:** 0–3 sound, 4–7 **silent** | — | identical `(O,L)`, opposite shape — the cleanest "which mode" test pair |
| A7 | O0 L4, `G` 8→4 | before 0–3 (4–7 silent) → after all 4 | — | abs1 gate is a pure function of `(step mod G, O, L)` — **drift-free by construction**, immediate on a global change; the drift bug is cycle-ONLY |

Throughline: cycle bounds by re-anchor, abs1 bounds by gate — **both bound by the global
phrase ring, neither caches a clamp, and in both the sounding extent is R-independent
(rotation is content).** The same `G`-sweep differential that *fails* for cycle today
*passes* for abs1 — one harness covers both. The LOR *values* (O, L) latch at the **global**
phrase edge in abs (abs addresses against the global — C1 refinement).

### Why all three modes exist — the expressiveness lattice
Model a phrase of `G` steps as a **gate** (which steps sound, a bit-vector in `{0,1}^G`)
and a **content** sequence (which cell plays at each sounding step). Then:

- **CYCLE:** gate ≡ **all-on**; content has **period `L`** — the `L`-window looped across the
  phrase (`L=4, G=8 → 0,1,2,3,0,1,2,3`). The ostinato / polymeter engine.
- **ABS1:** gate = a **single contiguous run `[O, O+L)`** (silence outside); content a
  **straight period-`G` read** within it (`s → (s+R−O) mod G`), played once, not looped. The
  placed-burst / articulation engine.

**Neither base mode subsumes the other:**
- *Unique to abs1* — a gate that isn't all-on (a contiguous **rest region**). Cycle has no
  base gate, so A1 (2–5 sound, rest silent) is unreachable in cycle.
- *Unique to cycle* — a **sub-phrase loop that still sounds everywhere** (content period
  `L < G`, gate all-on). Abs1 reads straight, so A6's `0,1,2,3,0,1,2,3` is unreachable in
  abs1.
- They coincide only at **`L ≥ G`**: both collapse to "play the whole phrase straight, all
  sounding" (cycle re-anchors before it can repeat; abs1's gate goes all-on). All the
  divergence lives in `L < G`.

**The painted overlay (the third kind) is the join.** It is an arbitrary mute mask ANDed on
top of *either* base: painted+cycle finally puts silence *into* a loop; painted+abs1 punches
non-contiguous gaps into the region. So the lattice is **cycle = sub-looping · abs1 = a
contiguous gate · painted = arbitrary gating**, and they **compose** — which is why the
engine keeps all three rather than collapsing to one. (All gating is output-only: it zeroes
GATE/CV out, never generation/correlation/marginals.)

### Painted overlay — addressing (DECIDED, Rodney 2026-11-18)
**The painted overlay is ABSOLUTE — a phrase-level gate, not a window-rider.** It is indexed
and rotated on the **current global phrase ring `G`** (re-interpreted each phrase), not locked
to the base window's loop.

- **Why absolute (expressiveness subsumption).** A cycle base of length `L` with an absolute
  mute mask of *period `L`* reproduces the window-rider case exactly (`(content, mute)` both
  period `L`, identical every loop). An absolute mask of any *other* period gives loop-vs-gate
  **phasing** — the same loop under different mutes each repetition — which window-rider
  cannot produce. So absolute ⊋ window-rider: it is the superset and the generative one (the
  varied-repetition the instrument is for), and it keeps the overlay an orthogonal
  phrase-level gate rather than coupling it to `L`.
- **Fork 2 (resolved by this):** "absolute" = absolute-within-the-current-`G`. The overlay's
  `pat >> phraseStep` indexing AND `rotMaskRing` move to `% globalLen` (not `% kMaskRing`),
  consistent with the abs1 ring ruling.
- **Fork 3 (abs2 vs abs3, both kept):** abs2 (`followR` off) = **static**, pinned to absolute
  phrase steps of the current `G` (never re-pins). abs3 (`followR` on) = **follow-R**, mutes
  rotate by `(R − anchorR)` on the `G`-ring — the mute travels with the content's rotation and
  composes with the cycle re-anchor. The anchor/re-pin law is unchanged (mask-step-1 pinned to
  base R; a real base-R edit re-pins; `Δ` resets to avoid double rotation).
- **Modulated `L`:** an absolute gate stays put while `L` morphs underneath — a stable gate
  over a morphing loop, which reads as music, not the chaos a window-riding mute would make as
  it stretched with `L`. The locked-muted-ostinato case (auto-track a modulated `L`) is the one
  thing absolute does not do for free; if it ever proves a constant need it is a later
  convenience toggle ("lock mask to window"), never the base semantics.

### Painted overlay — worked examples (P-series)
Mask bits: `1` = sound, `0` = mute; the mask is ANDed onto the base gate per phrase step,
indexed on the `G`-ring. The only mode-param is the rotation source: **abs2** (`followR`
off) static, **abs3** (`followR` on) rotated by `Δ = R − anchorR` so `overlay[p] =
anchored[(p+Δ) mod G]` — a painted mute at step `k` lands at `(k − Δ) mod G`.

| # | base + mask | result | the point (test seed) |
|---|---|---|---|
| P1 | cycle L4, G8 (content `0,1,2,3,0,1,2,3`), mask `1110 1110` (mute 3,7; period 4 = L) | iter1 cells `0,1,2`; iter2 cells `0,1,2` — identical | **mask period == L ⇒ every loop iteration identical** (absolute reproduces window-rider) |
| P2 | cycle L4, G8, mask `1110 1101` (mute 3,6; period 8 ≠ L) | iter1 `0,1,2`; iter2 `0,1,3` — **differ** | **period ≠ L ⇒ same loop, different mutes per repetition** (phasing / varied repetition). The two iterations *should* differ — this is the feature, **not** an A==B failure |
| P3 | mask `1101 1111` (mute step 2), `R: 0→1` | abs2: mute stays **step 2**; abs3: mute → **step 1** (`2 − (1−0)`) | abs2 muted-step is **R-invariant**; abs3 muted-step = `(k − (R−anchorR)) mod G` — travels with the content |
| P4 | abs1 region `[2,6)`, mask `1111 0111` (mute step 4) | sounds `2,3,5`; `4` muted; `0,1,6,7` silent | overlay ANDs the abs1 gate ⇒ **non-contiguous sounding inside the region** — unreachable by either base alone (this is "arbitrary gating") |
| P5 | mask authored on G8, then `G: 8→4` | overlay uses `phraseStep ∈ [0,4)` + mask bits 0–3; bits 4–7 **dormant**; `rotMaskRing` mod 4 | overlay sounding set = pure function of `(phraseStep mod G, mask, R)`; **zero residue**, dormant bits reappear when `G` grows back — drift-free like abs1 |

Throughline: the overlay is **absolute (phrase-indexed on `G`), composed by AND with the
base, with one knob — the rotation source**. P1/P2 are the locked-vs-phasing pair (the
reason absolute is the superset); P3 pins abs2-vs-abs3; P4 is the arbitrary-gate capability;
P5 pins drift-free re-interpretation under a global change. **The guard must allow P2's
per-repetition divergence** — same trap the cycle guard nearly fell into.

### Where it lives (fix surface)
- `voiceFrameTickAt` CYCLE branch — the frame-phase output must track the
  global-anchored position, not the raw congruence.
- `tickHolders` — the commit edge keys off the anchored frame, not `wrapMod(t0EffNow…)`.
- `SequencerEngine.cpp:629` — the scan modulus follows the re-anchored window (not the
  raw lane length).
- All three are the **same anchor** — make the commit edge and the scan track the live
  global-anchored position the read already uses; the window extent and scan bound follow,
  no separate `min()` clamp to store. This is the concept face of **CA5** (the phase-holder
  skeleton) and should land as one change, reproduce-first against the differential.
- **abs1 (separate, smaller):** `maskWindowContains` + the abs1 read — `% kMaskRing` → `%
  globalLen`, `L >= kMaskRing` → `L >= globalLen` (ring = the current global phrase). No
  drift fix needed here — abs1 is already a pure function of the phrase position.

### Reproduction / guard
`test/diag_global_dip_advoffset.cpp` (on `debug/global-dip-logging`) is the reproduction.

**The guard is NOT raw `A==B` after the dip.** `A==B` conflates two things: the bug (the
voice sitting off its own global-anchored schedule, latched at an off-grid step-6/7) and
*legitimate* divergence (the dipped engine genuinely played a shorter window, so the
probabilistic wander random-walks apart — and the **mono transport itself wanders**, since
`dMain` is a probabilistic FORWARD/BACKWARD/HOLD decision reading the global-bounded
variation lane, so `pos_.phrasePos()` is a view of a walk that legitimately differs A vs B).
You cannot un-play the dip; `A==B` on the offsets is the wrong criterion.

What to assert instead — a **within-engine** residue-free property (single engine, dip
16→4→16 under live modulation + a wandering voice):
- **On-schedule:** at every step, including across and after the dip, the lane-LOR commit
  fires **iff** the engine's own live global-anchored re-entry holds this step
  (`pos_.phrasePos()==0` in the global≤voice case; the voice's live anchored voice-step-zero
  in general). No commit at a step the live anchor does not sanction — **no latched
  off-grid step-6/7**. Checked against the engine's *own* current phrase position, so the
  mono's wander is baked in and never counted as a failure.
- **Regression floor:** the static-no-modulation differential stays byte-identical (a dip
  with nothing modulated changes nothing). Offsets are *allowed* to diverge under live
  modulation — that is the music, not a defect.

Rejected framings (they force `A==B` by making something ignore the global range, the
opposite of UAS nesting): free-running the wander's reads on the raw congruence (contradicts
Consequence 2 — the wander reads the *clamped* window); or shielding the mono from the dip
(reverses the C0 FAMILY-2 bounded-seed law; the mono *is* the global). Weaken the assertion
to the real property, never the engine to the assertion.

`MONSOON_DIP_LOG` on the same branch is the Rack confirmation, now optional.

---

## THE COMMIT EDGE — probability lanes move at the voice range edge (CC_FIX_CYCLE_COMMIT_WINDOW_EDGE, 638879aa)

**The property (Rodney, the definition that makes it verifiable):** a probability
lane is *allowed to move from modulation* exactly when **its own rotation aligns
with the voice range edge** — the **LEFT** window edge in **forward**, the **RIGHT**
window edge in **reverse**. Rotation and the step edge are distinct: rotation is a
*content* position (the rotation tick); the step edge is a fixed *box-offset*
position (0 left / L-1 right). The commit STEP shifts with rotation; the edge it
lands on does not.

**Landed (cycle mode, voice frame):** `voiceFrameEdgeLive_`'s CYCLE arm fired on
`vCoord==0` = box-offset R = the rotation tick, so the lanes moved when the voice
hit its ROTATION, not its range edge — in every build, 74cf5c14 included. The fix
fires when the playhead box-offset `wnow` reaches the edge (0 forward / L-1
reverse). Verified by R-sweep: the commit read-cell is the window-edge cell
(O fwd / O+L-1 rev) for ALL rotations, where before it tracked R.

**Why it hid for days:** the whole suite ran `R==0`, where rotation IS the left
edge — degenerate. `test_cycle_commit_edge` now SWEEPS R and pins the edge cell for
every rotation (reproduce-first: fails from R=1 on the rotation-phase engine).

### STILL OPEN (two, same off-by-one family, one tier down / one mode over)

1. **Reverse probability-lane rotation aligns to the WRONG voice edge.** With the
   voice-frame commit now at the voice RIGHT edge in reverse, the probability lane
   in reverse still hits its OWN rotation at the voice LEFT edge; it should hit it
   at the RIGHT edge (so prob-rotation ↔ voice-right-edge, per the property). This
   is the lane tier (laneRecomp / the lane read), not the voice frame fixed in
   638879aa. Forward is correct. Reproduce-first with an R-sweep at the LANE read
   (not the voice read); the fix is the analogous reverse edge at the lane tier.
   Confirm against the property: prob-lane rotation must meet the voice range edge
   (fwd left / rev right) for EVERY rotation.

2. **abs1 / absolute mode** — `voiceFrameEdgeLive_`'s abs1 arm
   (`voiceFrameReadsFullPhrase`) still keys on `vCoord==0` and almost certainly
   carries the same rotation-vs-edge bug. Needs a feel-test on absolute mode, then
   the analogous window-edge fix.
