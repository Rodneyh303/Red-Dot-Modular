# Accent as a 4th poly lane — design + staged plan

**Goal:** promote accent from a mono-only strand to a first-class POLY lane, modelled after
rest — each poly voice re-derives its own accent (own `accentProb`, own per-lane LOR, own
`polyAccentRandom` draw) instead of inheriting mono's. Follows established rest/melody/octave
patterns throughout. Branch: `feat/accent-poly-lane` off MergePPQNWork.

This is the planned "test of the VoiceResolver" — see §"Does the resolver help?".

## Current state (verified)
- Lanes: `enum PolyLane { PL_REST=0, PL_MELODY=1, PL_OCTAVE=2, PL_LANES=3 }`; arrays
  `polyLen/Off/Rot[15][3]`.
- Poly draws: `polyRhythmRandom / polyMelodyRandom / polyOctaveRandom [15][16]`. **No
  polyAccentRandom** — poly accent does not exist yet.
- Accent today: mono-only. `accentRandom[16]` master strand, `accentLockedA/CandB`, drawn in
  the rhythm group (per redrawRhythm: 16 × (4 mono {rhythm,variation,legato,accent} + 15 poly
  rhythm) = 304 unit() calls/draw). Accent feeds the gate-accent output at mono level; poly
  voices currently share/ignore it.
- Derivation model (rest, the template): each voice has `restProb`; draws
  `polyRhythmRandom[v][restIdx]` at its own rest LOR; rests if `r_rest < restProb`.

## Derivation decision (accent, modelled after rest)
Each poly voice gets its own `accentProb` + `polyAccentRandom[v][accentIdx]` at its own
accent-lane LOR. Poly accent fires per-voice on its own draw vs its own prob — NOT shared
from mono. (Mono accent path unchanged; it's just now voice 1 of the same lane structure.)

## Draw-count impact (touches the Philox regeneration!)
Adding 15 poly-accent draws changes the rhythm draw from 304 → 16 × (4 + 15 + 15) = **544**
unit() calls/draw. The order MUST stay fixed and identical between redrawRhythm and
PatternEngine::regenerateRhythmB (the Option-3 reload replay) or reload diverges. Insert poly
accent draws in a FIXED position (after poly rhythm, per step i). Pre-release: no saved-patch
counter compat concern, but the two code paths must agree. DRAW_CHUNK=1024 still > 544/draw,
fine.

## Staged plan (each stage its own commit; engine stages need the parallel-run harness)

### Stage 1 — ENGINE: add the lane (note path) ⚠ build+harness
- PolyLane: add `PL_ACCENT=3`, `PL_LANES=4`. Resize polyLen/Off/Rot[15][4] (~38 sites).
- Add `polyAccentRandom[15][16]`, `polyAccentLockedA/CandB[15][16]`, slewed/source arrays.
- masterLaneProbability/Step + polyLaneProbability/Step/AtStep: add PL_ACCENT case →
  accentRandom (mono) / polyAccentRandom (poly).
- redrawRhythm: draw 15 poly-accent values per step (fixed order, after poly rhythm);
  promote/step/source-rebuild parallel to poly rhythm.
- regenerateRhythmB: MIRROR the new draw exactly (same 544-call order).
- executePolyVoice: per-voice accent decision from polyAccentRandom[v] vs v.accentProb,
  at the voice's PL_ACCENT LOR. Define PolyVoice.accentProb.
- VERIFY: parallel-run harness — poly rhythm/melody/octave draws BIT-IDENTICAL to pre-change
  (accent draws are NEW, inserted after; if poly rhythm shifts, order is wrong). test_voice_
  resolver extended to assert PL_ACCENT reads.

### Stage 2 — RESOLVER: confirm it generalizes (the test)
- VoiceResolver already takes `lane` as a param → laneProbability(v, PL_ACCENT) should work
  with ZERO resolver changes once the engine handles PL_ACCENT. Update the comment listing
  lanes. Extend test_voice_resolver to cover PL_ACCENT across all 16 voices. THIS is the
  resolver payoff: the UI read path gets accent for free.

### Stage 3 — PERSISTENCE: accent A + regen ⚠ build
- Persist polyAccentLockedA (irreducible A, like the other lanes). Regenerate B.
- Per-lane LOR persistence: polyLen/Off/Rot now [15][4] — save/load the 4th lane.

### Stage 4 — SANDS MONO panel: accent spread knob + CV
- Add accent SPREAD knob + CV in (accent currently has no spread control). Mono is the
  template for the others' layout. gen_macro_mono.py / the mono panel generator.

### Stage 5 — SANDS EAST + MACRO panels: 4th lane + relayout ⚠ HP change
- Add 4th lane group (accent) to both: base + L/O/R + spread, parallel to rest.
- RELAYOUT left jacks+attenuverters to Sands-Mono style: one left-to-right row per lane
  (not the current two-rows-per-lane). Compress lanes horizontally to Mono's lane width.
- Squeeze the lower mix-in attenuverters/knobs horizontally to fit the 4th (accent) group.
- Likely minor HP increase. gen_east_clean.py / gen_macro_mono.py + the *.gen.hpp.
- Re-run gen_layout.py; regenerate panels; the [15][3]→[4] Ids + bindings.

### Stage 6 — STRAITS EAST + WEST expanders: accent column ⚠ HP change
- Add accent base + cv/atten + accent GATE OUT columns, parallel to rest. They're spaced
  out now → minor HP change should fit.
- New MonsoonIds for accent params/inputs/outputs; binds; engine wiring.

## Does the resolver help? (the experiment)
HYPOTHESIS: the resolver insulates the UI read path from the lane addition. Concretely:
- Prob-outs (East+Macro) read via resolver.laneProbability(v, lane) — adding PL_ACCENT to the
  lane loop is a one-line range change (3→4), no per-lane dispatch code, because the resolver
  already generalizes over `lane`.
- The mono/poly distinction for accent is handled by the resolver exactly as for rest — no new
  -1 remap or tab special-cases needed in the UI.
- Where the resolver does NOT help (honest): the ENGINE lane-count resize (38 sites), the
  draw-loop, persistence, and the PANELS — those are below/beside the resolver. The resolver
  is a read-addressing layer, not a lane-count abstraction.
Verdict recorded after Stage 2.

## Risk notes
- Stages 1, 3, 5, 6 need a build-capable session (note path + panels). Stage 2 + the harness
  extension are verifiable here.
- The draw-count change (304→544) is the subtle one — the regeneration replay must match.
- Lane-count resize is a "touch 38 sites in lockstep" change, like the tab remap was.
