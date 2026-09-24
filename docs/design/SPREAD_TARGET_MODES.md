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
- UI: a "Spread target" SUBMENU with one two-state row per spread lane (REST, MELODY, OCTAVE, ACCENT).
- Persist in JSON like the old flag did.
- CONFIRM: q-mix is a melody-family value lane with its own slewed buffers (slewedQmix /
  slewedPolyQmix) — does it get its own row, or follow MELODY's setting?

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
