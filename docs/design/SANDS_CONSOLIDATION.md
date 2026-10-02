# Sands consolidation — three modules to one + thin expanders (Rodney)

STATUS: DESIGN DIRECTION, confirmed. Gated on a capability audit (below). Reprioritises Sands panel
craft: REDESIGN one unified module, do NOT polish Mono/East/Macro that are to be dissolved.

Companion: **SANDS_ARCHITECTURE_CONSOLIDATION.md** is the earlier ANALYSIS (where the complexity lives —
`combineLOR`/`combineSpread` — and the 3/2/1 option comparison). It predates the correlation matrix and
so could weigh options but not decide; THIS doc is the decision, on the new argument (copula dissolves
the soup fear) + the maintainability evidence. Its `combineLOR`/`combineSpread` analysis and option-1
"single module, paged" sketch are directly useful for the BUILD.

## Why (the case, strongest argument first)
1. **Three implementations = three bug surfaces.** This session's Sands bugs were ALL single-module
   divergences: the lane-index bug hit one visual not the others; the Mono spread no-op was Mono-only;
   the MIX IN label drift was Macro-only. The same conceptual operation implemented three times
   slightly differently — and the divergence is where the bugs lived. One implementation cannot disagree
   with itself. The collapse REMOVES the structural cause of a whole bug class we have been fighting.
2. **The correlation matrix dissolves the "soup" fear** that justified the three-way split. Variation and
   legato were kept MONO because uncorrelated per-voice variation = mush (AVERAGE_POLY). With graded
   Gaussian-copula correlation, per-voice variation is tamed: +100% = locked to mono (== today), 0% =
   independent, -100% = interlocking. Soup is one corner of a continuous, CA-group-structured space, not
   the default. So the lane-uniformity that split Sands into reference(Mono)/poly(East)/global(Macro) is
   obsolete — ALL lanes can be poly + correlated.
3. **Advances past the meloDICER lineage.** MEX3 (the inspiration) made legato+rest poly but kept
   variation mono — correct FOR THEM because they had no correlation matrix. Our copula removes exactly
   that limitation, which is what makes the split obsolete. Not copy-then-diverge; resolving a limitation
   baked into the inspiration.
4. **The "lost weekends" are retired CODE, not lost KNOWLEDGE.** The hard Sands work established what the
   per-voice lane model must do (editing, LOR, lock, blend, latch) and where the three diverged — i.e.
   the spec the unified module must reconcile.

## Proposed topology
- **Sands (ONE)** — all lanes, poly, correlated (reference voice + correlated field in one editor).
  Absorbs Mono's V1-editing and East's V2-16 editing (same thing at different voice indices).
- **+ Global modulation expander** — Macro's global-LOR layer (Macro: `global` x40), for those who want it.
- **+ Taps expander** — Macro's tap outputs (Macro: `tap` x31), for those who want them.
Maps to how people think: per-voice lanes (base), modulate globally (expander), tap out (expander).

## Capability audit (absorbed / relocated / redundant / LOST)
From a code scan of the three visuals:
- **ABSORBED (no loss):** strand/lane editing, lock, LOR, base — the per-voice lane representation the
  unified editor provides by definition.
- **RELOCATED (no loss):** global modulation + taps -> the two expanders.
- **REDUNDANT (simplification):** the `latch` that arbitrates which module OWNS a lane's base when
  multiple Sands are present — with one module there is nothing to arbitrate. Confirm nothing downstream
  depends on the latch STATE existing.
- **THE ONE REAL LOSS:** **lane-owner switching across PLAYHEADS** (East's distinctive `lane owner` +
  `blend`). East can switch which lane/strand a voice follows, and this can switch between different
  PLAYHEADS. The unified model assumes ONE mono playhead (Mono already uses the same playhead per voice).
  Whether this matters depends entirely on whether multi-playhead lane ownership is a sound anyone uses.
- **VERIFY before committing:** (a) `blend` (East x9) is the SAME as the correlation/reference blend
  (reference + correlated deviation IS a blend) and not a second mechanism; (b) `lock` semantics (heavy
  in all three) move cleanly into the unified editor and don't depend on the three-module split.

Verdict: large complexity reduction (3 modules -> 1 + 2 thin expanders) for the cost of one niche
capability (lane-owner across playheads) + a migration (pre-release, allowed). Decisive trade.

## Sequencing
- Do the capability audit resolution (lane-owner decision; blend/lock verification) FIRST.
- Then redesign ONE Sands module (this is where the panel-craft effort for Sands goes — not into
  polishing the three).
- Migration: Mono/East/Macro become legacy; pre-release so breaking is acceptable.


## Known gap this consolidation closes: SPREAD on VARIATION / LEGATO
Noticed post-gate-work (Rodney): spread currently covers 5 lanes (REST, MELODY, OCTAVE, ACCENT, QMIX —
SpreadInterp.hpp:55). **VARIATION and LEGATO have NO spread** — they have mono `slewed` buffers but no
`slewedPoly*` buffers, no per-voice draw/slew path, and no per-voice spread UI. So extending spread to
them is NOT a 5->7 table-widen; it needs poly buffers + the per-voice slew/pre-remap path + UI — i.e.
exactly the "all lanes poly + correlated" work this consolidation does ONCE, uniformly. Adding it now to
the three-module Sands would be thrown away at the collapse. So: **do it as part of the consolidation,
not before.**

**Safety in the interim:** the gate work added the per-voice VARIATION/LEGATO *mechanism*
(`perVoiceArticulation`, default OFF — RHYTHM_BEHAVIOUR_TOGGLES.md) but WITHOUT spread/correlation, so
turning it ON today gives the UNCONTROLLED per-voice variation = the "soup". That is acceptable only
because it is behind an OFF-by-default flag. The CORRELATED (safe) version arrives when spread reaches
VAR/LEG at consolidation — at which point perVoiceArticulation is removed/replaced by the graded
correlation (mono reference + per-voice correlated, +-1). Do not promote perVoiceArticulation or ship it
ON until then.


## PROPOSED UNIFIED ARCHITECTURE (Rodney) — "East, completed" + delegated LOR
The consolidation is NOT a from-scratch rebuild. It is **East generalised to carry everything**, with
the other modules' distinctive bits absorbed as DELEGATION STATES or relocated. Target:

**Unified Sands = East, with:**
1. **Per-lane MONO mod input + attenuverters**, alongside East's existing per-voice (poly) mod input +
   attenuverters. The mono input is the global/shared-CV layer -> **absorbs Macro's global MODULATION
   role** (global = the mono input; per-voice = the poly input; both attenuverted).
2. **Per-voice LOR governed by each voice's LANE DELEGATOR**, choosing per voice: **follow voice 1**
   (take voice 1's LOR — voice 1 is the reference) OR **independent** (own per-voice LOR). Plus bulk
   **ALL-FOLLOW** and **ALL-INDEPENDENT** actions. -> **absorbs Macro's global LOR**: "global LOR" is
   simply the ALL-FOLLOW state (set voice 1, all follow). No separate global-LOR layer / params /
   reader-writer needed. Same generalisation as spread-follow-CA and correlated ghost placement:
   GLOBAL IS NOT A LAYER, it is the follow/+1 delegation state of per-voice.
3. **Mono = voice 1 of East** (reference-voice editing is just editing voice 1).
4. **Taps (pre/post send taps, two per lane: LOR + SPREAD, continuous 1.0=POST/0.0=PRE) = the TAPS
   EXPANDER** (relocated, per the expander plan). Preserve the continuous pre<->post semantics exactly
   (SANDS_OWNERSHIP_SPEC.md §9) — do not simplify to a 2-way switch.

**The one genuinely open item: lane-owner-across-playheads.** NOTE it may be the DELEGATOR
GENERALISED — a delegator already answers "whose LOR does this voice follow"; lane-owner answers
"whose playhead/lane does this voice follow". If the delegator subsumes lane-owner, it folds in rather
than being lost. **Audit must confirm this.**

## Capability audit — REFRAMED to verify this hypothesis
The audit is now sharper: not "enumerate cold and discover an architecture" but "**confirm the
proposed architecture above covers everything, or name what falls outside it**". Specifically verify:
- East + per-lane mono mod + atten covers ALL of Macro's global modulation. [expect: yes]
- Per-voice LOR + delegators (follow-v1 / independent) + all-follow/all-independent covers ALL of
  Macro's global LOR, with global == all-follow. [expect: yes]
- Taps relocate to the expander with continuous pre/post semantics intact. [verify not simplified]
- blend == the correlation/reference blend (not a 2nd mechanism). [verify]
- lock survives the collapse. [verify]
- **lane-owner-across-playheads**: is it subsumed by the delegator, or a separate used capability? If
  separate and used, how does the unified model carry it? [THE open item — Rodney decides if used]
Green-light the rebuild only when every row is resolved.


## PREFERRED — DROP MONO, keep East→Macro (Rodney, supersedes "East completed")
Even simpler than absorbing Macro into East: **delete Mono entirely; keep the EXISTING East + Macro
pair and their East→Macro delegation as-is.** The win is TOPOLOGY-RULE DELETION, not role-merging:
- **East already addresses ALL voices including V1** — V1 is just voice 1 of East. Mono existed only
  to edit V1 as "the reference"; with Mono gone, V1 editing is in East and the reference role is the
  follow/delegation target (voice 1), not a separate module.
- **A large part of the topology rules exist purely to COORDINATE three modules** — the latch, the
  Mono-present/absent conditionals ("if no Mono present, East may delegate lane 1", SANDS_OWNERSHIP_SPEC
  §5), ownership arbitration. Dropping Mono makes those conditions UNREACHABLE, so the rules delete
  themselves. Removing coordination complexity (rules that do no musical work, only manage module
  relationships) is the best kind of simplification.
- **Two modules, one relationship:** East (per-voice, all voices) + Macro (global), East→Macro
  delegation the SOLE relationship. No three-way mesh.
- **Less to BUILD than "East completed":** you are not rebuilding East to swallow Macro's roles — you
  keep the working East/Macro split and delegation, and just remove the third wheel.

**Audit (now minimal):** confirm Mono does NOTHING but edit V1 (which East already does). If Mono
carries anything else — a distinct interaction, mono-specific mode, or the lane-owner/"same playhead
per voice" capability — that must land in East first or be a deliberate drop. That single question
("what does Mono do beyond V1 editing?") is essentially the whole audit now.

(The earlier "East completed / per-lane mono mod + delegated LOR" framing above is SUPERSEDED as the
primary plan — Macro stays as-is rather than being folded into East. Its delegated-LOR idea may still
inform how East↔Macro delegation expresses "global", but Macro is NOT dissolved.)


## COST ANALYSIS RESOLVES TO ONE MODULE (Rodney)
"Drop Mono, keep Macro" is NOT free after all: Macro was built with FEWER lanes (variation + legato
were mono-only then). That no longer holds, so keeping Macro means **adding variation + legato lanes
to it — extra sends, extra taps, panel re-layout.** That is exactly the per-module expansion the
consolidation exists to avoid. What keeping Macro buys for that cost: only **follow-Macro's-LOR** —
and that is a SMALL advantage, recoverable anyway as all-follow-voice-1 within East (the delegated-LOR
reframe).

So the thing that made Macro worth keeping (a distinct, SMALLER global layer) stops being true once
lanes unify. A full-lane Macro is just "East's global sibling" — at which point two modules is
unjustified.

**DECISION: full collapse to ONE Sands module.**
- **Mono deleted** (V1 = voice 1 of East).
- **Macro dissolved into East** (global = ALL-FOLLOW delegation state of per-voice; no separate global
  layer, no Macro lane-expansion work).
- **Taps = the taps expander** (relocated, continuous pre/post semantics intact).
- Lane uniformity is then FREE (East is already per-voice all-lanes).
- Zero module-coordination rules (one module), vs the three-module latch/arbitration mesh.
Trade accepted: lose "follow Macro's separate LOR" (minor; = all-follow within East). This supersedes
both "keep Macro" framings above. The delegated-LOR + per-lane-mono-mod ideas from "East completed"
are HOW the single module expresses global; they are the build content, now inside one module.


## THE ACTUAL REQUIREMENT (Rodney) — per-lane global+poly mix, not a full matrix, not a module merge
Why the current East+Macro setup exists: so **East can grab GLOBAL modulation (from Macro) AND apply
per-voice POLY tweaking on the SAME lane.** That "global + per-voice on one destination" is the real
requirement; East-grabs-Macro is just the MECHANISM (there was no general primitive, so East reaches
over to Macro and sums its own poly).

The "modulation matrix" reframe was useful for diagnosis but is OVER-ENGINEERING as a build: the thing
actually needed is not an N×M matrix (big redesign, opacity UX hazard) — it is the SPECIFIC 2-source
mix the reach-over approximates: **per lane, a GLOBAL (mono) source + a PER-VOICE (poly) source,
attenuated and SUMMED.**

**So the fix is minimal:** each lane in the ONE unified module natively has BOTH a global (mono) mod
input and a per-voice (poly) mod input, with attenuation, summed. Then:
- East no longer needs to grab Macro — Macro's contribution IS the lane's global input, now local.
- The East→Macro delegation DISAPPEARS (the thing it reached for is local).
- Macro as a separate module is unnecessary (its whole job was being the global source East reached to).
This is the "per-lane mono mod input alongside poly" shape from earlier — now JUSTIFIED precisely: it is
the internalisation of the East→Macro reach-over, i.e. the minimal form of the matrix op the design
actually uses. NOT a full matrix, NOT a two-grid merge — just global+poly summed per lane, in one
module. Confirms the one-module decision above and specifies the modulation structure inside it.


## CHOSEN APPROACH (pragmatic, lower-risk) — kill Mono, keep+extend Macro
After weighing BUILD RISK (not just build amount): dissolving Macro into East means deep surgery on
East's working modulation structure (per-lane global+poly sum, delegators, internalising the
reach-over). Keeping Macro and ADDING lanes to it is additive and LOCAL — Macro already is the
global-mod module with send/tap structure; you extend a working thing by two lanes rather than
re-architecting two modules' relationship. Given a working instrument + release target (and this
week's reminder that touching working engine code breeds subtle bugs), additive-and-local wins.

**DO:**
- **Kill Mono** (V1 = voice 1 of East). Survives from every framing — unambiguously right; the
  Mono-present/absent coordination rules still largely collapse.
- **Keep Macro; add VARIATION + LEGATO lanes** so it carries the full uniform lane set (they are no
  longer mono-only). Extra sends/taps for the two new lanes.
- **Move the SEND knobs to the RIGHT of the panel** to make room for the two added lanes. This is the
  concrete blocker ("no space") solved without a full redesign.
- East↔Macro relationship stays AS-IS (the working reach-over), now over the full lane set.

**Honest trade:** this is the SAFE path, not the simplest endpoint. The one-module dissolve (above) is
cleaner in principle but deferred — the modulation-structure cleanup (per-lane global+poly, delegators)
is NOT done here, only the lane-uniformity + Mono removal. **The full dissolve-into-one-module remains
an OPTIONAL later refinement once this is stable.** Supersedes the one-module decision as the NEXT
build; keeps it as the longer-term option.

## Capability audit — now scoped to this path
Confirm: Mono does nothing but edit V1 (-> East voice 1); Macro's variation/legato lane addition needs
only the poly buffers + spread the lane-uniformity work provides; the send-knob relocation doesn't
break the pre/post tap semantics; lane-owner / same-playhead — does killing Mono lose it, or is it
East/Macro only?


## Macro subsumes Mono; generalise East to mono — symmetric two-module model (Rodney)
Two facts that settle and tighten the chosen approach:
1. **Macro ALREADY works in mono mode** -> Macro's function already SUBSUMES Mono's. Deleting Mono
   rehomes nothing; Mono is pure redundancy. (Removes the last hesitation about the deletion.)
2. **East is currently poly-only, but should be generalised to work for a MONO Monsoon (no Straits).**
   Then BOTH modules span mono AND poly, and "mono" stops being a module/mode — it is just the 1-voice
   CASE of the general thing (same as everywhere else in the instrument: mono is N=1, not special).
   Build is additive: East's poly path already handles N voices; mono = N=1 — likely nearly works if
   East is written generically over voice count. Audit: does anything in East assume N>1?

**Settled model (symmetric, Mono-free):**
- **Macro** — GLOBAL control, any voice count (mono or poly).
- **East** — PER-VOICE / fine control, any voice count (generalise to include mono Monsoon).
- **Both together** — combine via the existing delegation + sends.
- **Mono is a voice-count CONTEXT, not a module.** Macro and East each handle the full range.

User story (manual-ready): *"Macro for global, East for per-voice, both together for the combination —
each works whether your Monsoon is mono or poly."* Generalising East removes the last asymmetry
(East-poly-only) so the two-module story is complete and symmetric.
