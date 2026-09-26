# Spread target modes — graded correlation (Rodney)

STATUS: DESIGN, DECIDED. Two modes, per lane, Monsoon context menu. Not built.

## The change in one line
Spread's interpolation TARGET is hardwired to voice 1. Make it selectable — and in particular let it
FOLLOW CA's pin map — so CA authors WHICH voices are related and spread grades HOW MUCH.

## Where it stands today
`SpreadInterp` interpolates each voice's draw toward the MONO (voice-1) draw of the SAME lane:
`r = (1-|a|)·d + |a|·t`, with `t = p` for positive spread and `t = 1-p` for negative (the mirror).
A voice targeting itself is a no-op, so voice 1 is a fixed anchor. Full spread = unison with V1;
full negative = exact complement of V1. Amount is already PER (voice, lane).
(The former AVERAGE_POLY target was removed — the mean of N iid draws concentrates at 0.5, so full
spread collapsed everything to mush. Targeting the mono draw gives unison, a real destination.)

## Two target modes (DECIDED — Rodney)
1. **Anchor V1** — today's behaviour. Default.
2. **Follow CA** — anchor = `src[v]`, the voice CA says v is correlated to. Spread then means "how
   strongly do I adhere to MY OWN leader". This is the mode that pays off.

**A user-chosen fixed voice (anchor = any voice k) was CONSIDERED AND DROPPED.** It bought only an
anchor other than V1 when CA is absent — and CA is core to the suite, not optional, so anyone reaching
for graded correlation has it. Where CA IS present a fixed arbitrary anchor is strictly WORSE than
follow-CA: it can't track the leader when pins move, and it gives one hub where follow-CA gives the
whole structure. It also cost a 1..16 target-voice submenu on top of the mode item. Little is lost.

Dropping it CLOSES two open questions: there is no target INDEX any more (so no "which frame is it named
in" problem — the anchor is read from `src[v]`), and the fallback rule shrinks to "CA absent, or no table
for that stream → anchor V1", i.e. exactly today's behaviour.

## Why follow-CA is the right shape
**CA sets WHO (binary, one source per voice). Spread sets HOW MUCH (continuous, signed).** Neither
gives graded correlation alone. Follow-CA gets per-voice targets WITHOUT Sands authoring a single voice
relationship — CA stays the sole author of the voice frame; spread supplies only the scalar.

## Reachable correlation structures (worked)
With `k(a) = a/√((1-a)² + a²)` (so a=0.5 → 0.71 — the mapping is concave; most travel happens early):
| CA state | Structure | Notes |
|---|---|---|
| Identity diagonal | Diagonal — no correlation | spread is a no-op (every voice targets itself) |
| Today (fixed V1) | **Signed rank-1 star** | one common factor; range already -1..+1 between any two voices |
| Collapse / manual pins | **Block-diagonal, rank-1 per block** | arbitrary PARTITION, per-voice strength+sign, zero across blocks |
| Scatter (cycles) | **Neighbour-only coupling** | e.g. all a=0.6 in a cycle → 0.46 adjacent, 0 otherwise (banded, not a star) |

**Not reachable, and that's correct:** an arbitrary correlation matrix. Within a block it stays rank-1,
so `corr(1,2)=0.8, corr(1,3)=0.8, corr(2,3)=0` is impossible. Reaching it needs per-voice MIXES of
sources (a weighted 16x16 matrix) — which is both mush and far too many params. Rank-1-within-a-block
encodes exactly what the ear tracks (grouping, leading/following, together/opposed), which is the shape
real ensembles have. The restriction is doing musical work.

**Cross-lane correlation stays ZERO and is unreachable by spread** — each lane targets its OWN lane's
anchor, and the streams are domain-separated in the RNG. Voices can become jointly identical across all
lanes (unison), but rhythm never becomes correlated with melody. Cross-lane coupling would need a shared
SOURCE between streams — a different mechanism. (Rodney's separate call: cross-stream correlation is not
wanted; see CHANGE_ALLEY_DESIGN.md.)

## Musical payoff
- **Ensemble tightness** — per voice, per lane, how strictly a player follows their section leader:
  unison → loose → independent → answering. A control real ensembles have and modular usually doesn't.
- **Antiphony (the headline)** — two independent choirs (e.g. V2,V3 → leader V1; V5,V6 → leader V4),
  zero correlation across. The call-and-response is NOT scripted: two uncorrelated patterns take turns
  occupying the bar. Not reachable today (one hub ⇒ every voice tied to V1, no second group to answer).
- **Interlocking parts from one number** — negative spread makes a voice play the complement of ITS OWN
  leader, so a section can hold adherents AND an answering voice with no extra mechanism.
- **Gradient/smear** — a scatter cycle gives similarity that decays with distance across the voice field
  (spatial if the voices are panned), rather than a hub-and-spokes texture.
- Soloist + accompanists with individually dialled adherence (the classic star, but anchored anywhere).

## Boundary: single anchor, NOT a per-voice target map
Spread gets ONE anchor per lane (or per module). A per-voice target map (v follows t(v)) IS the CA pin
matrix; rebuilding it in Sands would duplicate CA and break the one-authoritative-voice-frame rule.
Follow-CA gets the per-voice behaviour by READING CA, not by re-authoring it.

## Could spread have replaced CA? No.
At a=1 with per-voice targets spread reproduces the MAPPING, but not: the VERBS (collapse/rotate/
reflect/scatter — operations on the mapping), REVERSIBILITY (Philox counters, true-reverse replay),
the STAGE (CA reorders source assignment UPSTREAM so gates/rests/accents/pitch follow coherently and
Intertropical + Keppel inherit it; spread blends values late, per lane), or REACH (the correlation pairs
permute EXTERNAL poly CV; one CA can serve several Monsoons). They are orthogonal axes of one idea:
CA discrete/structural/upstream/authored-once; spread continuous/value-level/downstream/per-voice-lane.

## OPEN — how to switch mode (the one unresolved question)
PRECEDENT: the earlier 2-option spread target (Average Poly / Mono Draw) was a **Monsoon CONTEXT MENU**
item and the **single source of truth**, mirrored onto the engine so every visual SpreadManager read it
(replacing per-visual `interpUseMono` flags). When AVERAGE_POLY was deleted the field went with it —
`Monsoon.hpp:658` still carries the ORPHANED COMMENT describing it (clean that up).
DECIDED: reuse that pattern — **Monsoon context menu, single source of truth, mirrored to the engine**
(never per-visual flags: that is what diverged last time), **PER LANE**.
- **PER LANE, not global (Rodney).** The lanes are independent, so "melody follows CA, rhythm stays
  anchored to V1" IS the musical control; one global setting would couple decisions with no reason to be
  coupled. Cost is a few menu rows — cheap against that.
- UI: a "Spread target" SUBMENU with one two-state row per spread lane — REST, MELODY, OCTAVE, ACCENT
  and **QMIX**.
- Persist in JSON like the old flag did.
- **Q-MIX GETS ITS OWN ROW (Rodney)** — same treatment as every other lane, not shared with MELODY.
  It is first-class throughout (own Philox stream, own dice/reseed, full Raffles parity, its own
  slewedQmix / slewedPolyQmix buffers); a shared row would be the one exception. So 5 rows, and
  q-mix can follow CA while melody stays anchored to V1 (or vice versa) — which is the point, since
  q-mix is the per-voice seq<->quantise blend and correlating WHICH voices sit where on that axis is
  exactly what the QM correlation pairs are for.

## Other OPEN items (small)
- **Keep the anchor PRE-spread** (as the code does now): correlation is then one hop only and cycle-proof.
  Using the leader's POST-spread value would propagate transitively and become ill-defined the moment
  CA's pins form a cycle — which a permutation easily does. One hop is the deliberate choice; its cost is
  that a chain 1→2→3 gives corr(1,3)=0.
- Manual pins are overwritten when a verb fires on that stream — lock mode protects a hand-authored
  partition.
- Clean up the ORPHANED comment at `Monsoon.hpp:658` (describes the deleted Average Poly / Mono Draw
  field) when this lands.

---
Origin: the idea arrived on Rodney's walk to the MRT, morning commute.

## Time-varying structure (everything above is a SNAPSHOT)
Three timescales, and the split is what keeps it audible:
- **Amounts glide** — spread amounts are CV-modulatable, so ensemble TIGHTNESS moves continuously.
- **Shape steps** — verbs mutate the pin map at PHRASE BOUNDARIES, so the GROUPING jumps: a section
  becomes a chain, two groups merge, a leader changes.
- **Trajectory reverses** — true reverse (CA_DICE_COUNTER_MODEL.md) walks the committed shapes BACKWARD.
  The sequence of structures becomes a playable object; that is form, not modulation.
The discrete/continuous split is not a limitation — the ear needs a grouping to PERSIST to hear it as one.
Stable within the phrase, re-dealt between, is exactly what makes the structure perceptible; continuously
morphing maps would read as mush.

## Correlated spread modulators (via the CA poly in/out pairs) — closes the circle
Patch any modulation into a CA pair's poly IN, take the poly OUT into the SPREAD AMOUNT CV. Each voice's
ADHERENCE is then modulated by the voice-permuted version of that signal: the STRENGTH of correlation is
distributed by the same table that defines WHO is correlated.
- **Which pair you tap sets the character.** A RHYTHM pair permutes adherence the way the gates are
  permuted → "tighten whoever currently carries the groove". A MELODY pair → "tighten whoever carries
  the line". Genuinely different musical statements.
- **Self-referential version:** feed a MELODY pair's output into the MELODY lane's spread amounts. The
  correlation's own permutation then decides how strongly each voice adheres to that same correlation —
  when CA scatters, grouping and tightness reshuffle in lockstep. The structure grades itself. (This is
  the level-crossing shape the instrument keeps producing: the correlator's output controlling the
  correlator.)
- **Bipolar caution:** spread is signed, so a bipolar source makes voices flip between adherent and
  MIRROR as it crosses zero — strong; usually wants attenuation.
- **Frame rule applies:** if the voices are arranged by Intertropical, take the spread modulation from
  the INTERTROPICAL tap, not the CA tap — otherwise tightness is distributed in VOICE space while the
  notes are in PART space ("tap at the same stage your carrier comes from",
  CORRELATED_POLY_MODULATION.md).
Uses only things that already exist once the CA pairs are built.

---

## CORRECTION (Rodney): follow-CA as first specced is a NO-OP — and the fix

**The bug.** `PatternEngine.hpp:139`: CA's pins remap the SLEWED buffers **post A/B-mix, post-slew,
PRE-spread**. So by the time spread runs, voice v ALREADY holds `src[v]`'s material. Under follow-CA the
anchor would be `src[v]` — the value the voice is already carrying — so `d == t` and
`r = (1-a)·d + a·t = d` for EVERY amount. Pin voice 2 to 3, the editor shows 3, and spread does nothing.
Caught before build.

**The fix: keep the voice's OWN pre-remap draw available at the spread stage** (one extra
16 x lanes buffer — trivial). Spread then interpolates between the two endpoints that the remap
currently destroys one of:
```
own    = this voice's own pre-remap slewed draw
leader = the post-remap value (i.e. src[v]'s material)
r      = (1-|a|)·own + |a|·(a >= 0 ? leader : 1 - leader)
```
Alternative considered and NOT chosen: move the remap AFTER spread. Conceptually cleaner but reverses an
ordering everything downstream assumes — higher risk for no extra capability.

**Polarity — PROPOSAL 1 restored (Rodney's call, and the intellectually satisfying one):**
| amount | result | correlation |
|---|---|---|
| `a = 0`  | the voice's OWN draw | 0 — uncorrelated |
| `a = +1` | its LEADER's material | +1 — full adherence |
| `a = -1` | the COMPLEMENT of its leader | -1 — interlocking / hocket |
Clean separation, one semantics per control: **the PIN says WHERE, SPREAD says HOW MUCH, the SIGN says
opposition.** This keeps interlocking (kotekan/hocket), which the "complement of own draw" variant would
have lost — the complement of an independent draw has no audible relationship to the leader.

**Consequence to design around (NEW behaviour, not a regression):** on a follow-CA lane the pin alone
now does NOTHING until spread is dialled up — CA's verbs become inaudible at `a = 0`. That is correct
under "spread = how much", but it will read as "CA is broken" on a fresh patch. Mitigation: when a lane
is switched to follow-CA, initialise that lane's spread amounts to FULL (+1) so enabling the mode
preserves today's "pins take effect" feel, and the user dials DOWN to loosen. [OPEN - confirm]
(Back-compat is a non-issue: no public release yet.)

**Anchor-V1 mode is still needed** — with no CA in the chain `src[]` is identity, so every voice targets
itself and follow-CA is a no-op by construction. Anchor V1 remains the sensible default for CA-less racks.

**Editor display note:** because the remap is pre-spread, the Sands editor shows POST-CA material — a
voice pinned to another displays that other voice's bars. Worth saying so in the lane tooltip, since the
per-voice spread amount is then being set on material that is not that voice's own draw.


---

## Spread menu: per-mode defaults + the "needs a CA" cue (Rodney)

**Per-lane menu (Rodney) — one submenu per spread lane (MELODY / OCTAVE / QMIX / REST / ACCENT):**

```
Target      (o) Anchor V1        ( ) Follow CA      <- greyed + reason if no CA reachable
Default when Anchor V1     -1  /  0  /  +1
Default when Follow CA     -1  /  0  /  +1
[x] Apply default on mode change
    Apply default now
```

- **Defaults are three-valued: -1 / 0 / +1**, matching the polarity landmarks exactly, so a default
  is simply "which landmark does this lane start at": `-1` oppose (complement of the target),
  `0` independent (the voice's own draw), `+1` follow (the target's material). Coarse on purpose —
  it is a STARTING POINT; per-voice fine-tuning happens on the Sands amounts as now.
- **A default per mode, both user-selectable**, because the sensible starting points are OPPOSITE
  and one shared default would break a direction: `follow CA` wants `+1` (enabling the mode then
  preserves today's "the pins take effect" behaviour and the user dials down to loosen — at `0` the
  voice plays its own draw, so CA's verbs would go silently inaudible), while `anchor V1` wants `0`
  (today's behaviour; `+1` would slam voices toward unison on switching). Ship those as the initial
  values; both remain editable.
- **`Apply default on mode change` is a TOGGLE, default ON.** Applying on every switch is
  destructive — someone who has hand-dialled 16 voices and flips modes to compare would lose the
  lot — so it must be defeatable. With it OFF, mode switching never touches spread values.
- **`Apply default now` is an explicit action**, always available. It is a normal param change, so
  Rack's undo covers it — which is why the destructive operation belongs behind an explicit
  invocation rather than a side effect.
- **Scope: EVERYTHING here is PER LANE (Rodney)** — the target, both defaults, AND the
  `Apply default on mode change` toggle. Lanes are independently configured throughout this design,
  so the toggle follows suit.
- **`Apply default now` is available at ANY time (Rodney)**, not only around a mode change — that is
  the point of having it as well as the toggle. The toggle covers the automatic case (entering a
  mode with a sane starting point); the action covers the manual case (a lane has been dialled into
  a mess and wants resetting WITHOUT touching its mode), which the toggle can never do. It also
  means turning auto-apply OFF costs nothing: the operation stays one click away.
- **Which default does `Apply default now` use?** The CURRENT mode's default — the one in view and
  the one the lane is operating under. State it in the item, e.g.
  `Apply default now (Follow CA: +1)`, so it is unambiguous when the two defaults differ.
- **Optional convenience**: an `Apply defaults to all lanes` at module level. Five lanes means five
  menu visits to reset after an experiment; cheap to add once the per-lane action exists.
- **Advisory cue instead of silent inertness**: when a lane is in `Follow CA` with all spreads at 0,
  note it in the menu (e.g. `Follow CA — spread is 0, so pins have no effect`). Inform rather than
  mutate; same pattern as the "needs a Change Alley" cue below.

**"Needs a Change Alley" cue — advisory, not preventive.** With no CA in the chain `src[]` is
identity, so every voice targets itself and follow-CA is a NO-OP by construction. Silently doing
nothing is the worst outcome for a feature whose whole point is invisible structure: the user
concludes it is broken.
- GREY the menu item and append the reason, e.g. `Follow CA — needs a Change Alley in the chain`.
- **Still allow it to be selected**, and persist it. Blocking selection would make the setting
  depend on module order and get lost when CA is absent — exactly the state loss the connection
  rework has been removing. It simply starts working when a CA is added.
- Reuse the connection model for the check: CA reachability is already answerable via the manager's
  `cachedChangeAlleyV2`; do not invent a second discovery path. The same grey-out-with-reason
  convention should apply anywhere else a mode depends on a module being present.
