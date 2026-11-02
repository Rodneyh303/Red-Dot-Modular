# STEPS_32 — the resize plan (32 the ONLY mode; no 16/32 switch)

Companion to STEP_COUNT_DECISION.md (the why: long-note arithmetic cap, Melodicer precedent, ceiling
set at 32 by UI practicality + copula cost) and LIGHT_RING_32_STEPS.md (the ring plan). This doc is the
HOW: the site-by-site resize inventory, the laws, the phases, the gates. All file:line cites verified at
lane tip 81fc7826 (re-grep at implementation time; the feat/position-holders commit shifts
SequencerEngine*.cpp/hpp lines and touches NONE of the storage/mask/surface sites below).

## Decision: one mode — a ceiling raise, not a switch

**32 is the only mode. There is NO 16/32 mode bit anywhere.** The length control is and stays
`PATTERN_LENGTH_PARAM` (variable length already exists, 1..16 — MonsoonConfigurator.cpp:30); "16" is
just `length <= 16`. A separate switch would be a second copy of the same information that can disagree
with the knob (length 20 in 16-mode?), would need mode-dependent semantics in save/restore, linked
expanders, hit-testing and every subsystem, and is inconsistent with today's model (there is no 8/16
switch; 16 is not special). The surfaces are static SVGs + widget overlays: 16/32-optional would mean
TWO asset sets + runtime widget swap; 32-only means regenerating the assets once. The one benefit of a
switch — bigger cells for 16-only users — was already declined when the ceiling was set (32 cells stay
clickable at current HP; 64 is where fiddliness starts). Defaults stay 16 (D2): fresh patches and
existing patches feel identical until someone reaches for 17–32.

## The taxonomy — there are FOUR different 16s

Every `[16]`, `& 0x0F` and `% 16` in the tree is ONE of these. The audit classifies before touching:

1. **STORAGE 16 — the phrase frame** (array step dimensions, frame bitmasks, frame-mod literals).
   **Extends to 32.**
2. **MUSICAL 16 — the bar of 16ths** (quarter-note anchors, beat phase). **INVARIANT — stays 16.** At
   32 steps the pattern is two bars; the bar does not grow.
3. **VOICE 16 — the 16 voices** (kVoiceSlots, voice routing, per-voice planes). **UNTOUCHED.**
4. **PARAM-RANGE 16 — length 1..16, offset 0..15** (knob ranges, CV formulas, LOR_LEN values).
   **Extends to 1..32 / 0..31** (CV formulas: decision D1; defaults unchanged: D2).

### Corrections to STEP_COUNT_DECISION.md's inventory (this audit)

- **caRhythmSrc / caMelodySrc / caQmixSrc `[16]` (PatternEngine.hpp:282/283/288) are NOT steps** —
  they are VOICE→VOICE routing maps. Proof: `caSrcRow(row, strand)` (hpp:312–330) indexes sources by
  row and reverts OOB against `numPolyVoicesHint + 1` ("Square matrix: source & target OOB together").
  **UNTOUCHED by 32.**
- **containmentParents[16] (hpp:303) is NOT steps** — "Bit u of containmentParents[v] = voice v is
  contained in voice u" (voice-indexed bits, json'd verbatim per CantonmentModuleCore.hpp:125).
  **UNTOUCHED.**
- The core store is at PatternEngine.hpp:**108** (doc said :103 — drift).

## The site inventory (verified at 81fc7826)

### A. The storage frame (arrays + types)

| Site | What | Change |
|---|---|---|
| PatternEngine.hpp:108 | `random_[16][NUM_STRANDS][16]` — the unified store (voice×lane×step) | step dim → 32 |
| hpp:180 | `warped_[16][NUM_STRANDS][16]` — the warp mirror | step dim → 32 |
| hpp:124–130 | 7 mono reference views `float(&)[16]` bound to `random_[0][lane]` | auto-resize with the store (retyped, no logic change) |
| hpp:158/163/198 | `polyRandom` / `warpRow` return `float(&)[16]` | → `float(&)[MAX_STEPS]` |
| hpp:368–369 | `preRemapSlewed{Rhythm,Variation,Legato,Accent,Melody,Octave,Qmix}[16][16]` (voice×step) | step dim → 32 |
| hpp:403 | `mR[16], mM[16], …` per-strand step buffers | → 32 |
| hpp:409 | `static thread_local pR[16][16] …` poly buffers (voice×step) | step dim → 32 |
| hpp:627–628 | `slewed{Rhythm,Variation,Legato,Accent,Melody,Octave}[16][16]` (voice×step) | step dim → 32 |
| hpp:669 | `rhythmSource[16][16]` + the sibling `*Source` arrays (voice×step) | step dim → 32 |
| SequencerEngine.hpp:966 | `uint16_t windowMask = 0xFFFF` — the window play mask (bit = frame step) | → `uint32_t`, init `0xFFFFFFFF` |

Voiced 16s that STAY (do not touch): `cachedPolySpread[16][7]` (hpp:115, voices×lanes),
`lorStore_[kVoiceSlots][NUM_STRANDS][LOR_ITEMS]` (SequencerEngine.hpp:506 — items, not steps), the
CA source maps and containmentParents above.

### B. The mask planes (the absolute-2/3 play/mute masks)

| Site | What | Change |
|---|---|---|
| SequencerEngine.hpp:578–592 | `maskPainted_ / maskAnchored_ / maskLastPushed_[kVoiceSlots]`, `maskMacroPainted_/Anchored_/LastPushed_` — all `uint16_t`, init `0xFFFF` | → `uint32_t`, init `0xFFFFFFFF` |
| hpp:595–598 | `rotMask16(m, delta)` — the 16-bit rotate (content direction) | → `rotMask32`, `delta` mod 32 |
| hpp:599–602 | `maskWindowContains(O, L, phraseStep)`: `if (L >= 16) return true` | `L >= MAX_STEPS` (frame-cover test); `s` mod 32 |
| hpp:606–612 | `windowMaskBits(L, O)` — clamps `L` to 16, builds the 16-bit mask | clamps to 32, builds 32 bits; callers hpp:705/:714 flow unchanged |
| Monsoon.hpp:968–972 | The editor float rows: `maskEastPatternBits[16]` etc. — per-voice floats carrying a 16-bit pattern (`65535.f` default) | **Float precision wall:** 2³²−1 is not float-representable. → hi/lo float pair (`…BitsLo[16]` + `…BitsHi[16]`, hi default 0). ≤16 semantics bit-identical (hi word 0); json rows carry two keys (D4) |

### C. The frame literals (each classified; & 0x0F → & 0x1F for frame masks)

SequencerEngine.cpp: isStepInWindow :233; setWindow :236–251 (`endStep` wrap :240, mask build :244,
reseat :249); the backward/forward walk bounds :475–488; `macroFrameRead_` :643; `voiceRead_` :681/:697;
the snap path :779–792 (incl. the 16-bounded window search :786); `updateWindow` offset mask :800/:806–808;
rel-light/trail arithmetic :849/:850/:880/:898; `refreshPolyRandomCell` :1653–1660; :1769/:2175/:2213.
PatternEngine.cpp:256 (warped_ cell write) and PatternEngine.hpp:545 (warpRow read) — `step & 0x0F`.
SequencerEngine.hpp:596/:601/:608 (phrase-delta helpers, mod-16 with +16 bias).
Monsoon.cpp:1148 `phaseStep = (absStep % 16 + 16) % 16` — **MUSICAL 16, STAYS** (bar phase).
StraitsSandsMacroVisual.cpp:1280/:1306 `calcPlayhead(...) & 0x0F` — frame value → 0x1F.

**The one genuine ≤16 semantic shift (D3):** the window WRAPS the frame today — offset 15 + length 16
plays the rotated full ring [15,0,…,14] (endStep `(15+16−1) & 0x0F` = 14). At frame 32 the same config
extends instead ([15…30]) — "length 16 = the whole ring" becomes "length 32 = the whole ring". This is
inherent to growing the ring (the rotated-order semantics survive; the full-ring-at-16 identity does
not). Preserving the 16-wrap would reintroduce frame-16 logic — a 16-mode in disguise. Accept + pin
old-vs-new in the new suite.

### D. Length/offset plumbing

| Site | What | Change |
|---|---|---|
| MonsoonConfigurator.cpp:30/31 | `configParam(PATTERN_LENGTH_PARAM, 1, 16, 16)`, `configParam(PATTERN_OFFSET_PARAM, 0, 15, 0)` | ranges → 1..32 / 0..31; **defaults stay 16 / 0** |
| SequencerEngine.cpp:797–809 `updateWindow` | knob `clamp(round(len),1,16)`; length CV `round(cv/10*15)+1`; offset `& 0x0F`; offset CV `round(cv/10*15)` | knob clamps → 32/31; CV formulas → `*31` per D1 |
| SequencerEngine.cpp:146/:155/:183/:191/:201/:207 | Fresh-init `cachedLength = 16`, `LOR_LEN = 16` everywhere | **STAY 16** (fresh patches identical; 16-lane inside a 32-frame is a legal nested length) |
| LOR_LEN value range | lane lengths 1..16 authored via the editor pushes | range → 1..32; the position-holder walk keys are `dir*64+len` — holders already take len > 16, re-anchor on change |

### E. Per-step loops (each `for(…16)` classified by induction variable)

PatternEngine.cpp: `rebuildWarpMirrorAll` :203–207 (v×l×**s**); the s-family :219/:228/:240/:248; the
draw/source-cache `i`-loops :433+/:45/:95/:119/:501–765 region. SequencerEngine.cpp: the
publish/refresh families. **Rule:** voice-loop (v) → stays; step-loop (s/i over the frame) →
MAX_STEPS, or live-length-bound where the algorithm allows (THE LENGTH LAW). Monsoon.cpp:1636–1639
`updateWindow` call site: no change (values flow).

### F. Surfaces

- **Ring:** embed_monsoon.py:32–33 — 16 `STEP_LIGHT_{i}` anchors at r=14 → 32 anchors (enlarged radius
  per LIGHT_RING_32_STEPS: position-only, no pattern-detail role, red-quarter/green-rest kept).
  Monsoon.hpp:500–503 `STEP_LIGHTS_END = START + 16` → `+ 32` (offset-based enum re-seats the
  downstream ids automatically). MonsoonWidget.cpp:474–475 bind loop 16 → 32; the `i % 4` red rule is
  MUSICAL (quarters) — unchanged, now 8 red anchors over two bars.
- **Sands grid:** StraitsSandsMacroVisual.cpp:388/:460 `stepW = (ED_W − 12)/16` → `/32` (cells halve;
  the accepted HP cost). Widget-drawn = no SVG regen for the grid itself.
- **Lantern:** Lantern.cpp:499 `N_STEPS = 16` → 32 (nvg-drawn, no assets); :1118 bar math (barW = grid/4)
  is MUSICAL — stays 16.
- **Panel SVGs:** Monsoon ring regenerate (2 themes); the lane/subgate panels are per-VOICE — untouched.

### G. Persistence

MonsoonPersistenceManager.cpp: the `loadPolyArr` family (~:430–441) reads the legacy 16-wide keys
(`polyRhythmRandom`…) into `polyRandom(…)[i]`, `i` < 16 — **keep the loaders 16-bounded**: legacy
patches fill [0..15], [16..31] stay at init → legacy-identical. `rebuildWarpMirrorAll()` after load
(:448) flows unchanged. Mask rows: hi/lo keys per D4 (legacy single-key rows read into Lo, Hi = 0).
No migration, no version bump needed — length/offset are params; old saves load and play identically
modulo D3-window configs.

### H. What does NOT change

Philox (draws are counter-keyed stream positions — rhythmDrawCtr/melodyDrawCtr — not step-indexed;
`dataFromJson`'s seed-counter-regen flow untouched). The hot path (step-rate reads index ONE cell;
nothing per-sample scales with the frame). The UAS/position-holder transport (wraps are LOCAL by each
lane's own length, SequencerEngine.cpp:428; the counter is dead; a len-32 window is just another len
value to the walk machinery — the non-div 7/5/6 cases were strictly harder than 32). Change Alley /
CA routing (voice-domain). Intertropical, scales, q-mix, subgate panels (voice-domain surfaces).

## The laws

- **THE MODE LAW:** one mode; `PATTERN_LENGTH_PARAM` is the only length control; ranges extend,
  defaults stay (16 / 0); no 16/32 bit anywhere, no dual assets.
- **THE LENGTH LAW:** step loops bound by live length where the algorithm allows; frame-bound
  (`MAX_STEPS`) only where the frame IS the data (window masks, warp mirror rebuild, whole-array
  publishes). Cost at len ≤ 16 must stay what it is today; the accepted doubling is confined to
  frame-bound control-rate recomputes at len 32 configs.
- **THE EXTENSION LAW:** every per-16 musical semantic (bar phase, quarter anchors, CA voice routing,
  Philox streams) is INVARIANT; steps ≥ 16 reuse them unchanged (32-as-extension-of-16). No per-32 map
  is introduced unless a site's audit proves it needs one (none found).
- **THE GOLDEN LAW:** every existing digest holds bit-identically at len ≤ 16 AND offset+len ≤ 16.
  The two documented shift classes — D1 CV formulas (if extended) and D3 window-topology at
  offset+len > 16 — are baseline re-captures, enumerated site-by-site with one-line reasons, house
  precedent: the subgate draw-source fix ("digests shift (baseline, not regression) — re-capture on
  the fixed base").
- **THE WARM LAW:** a mid-run 16↔32 window change is the `setWindow` re-author path — the
  position-holder re-anchor law already makes it bit-identical against f(t, newParams) (holders shadow
  `dir*64+len`). No new transport state. test_position_holders' re-anchor pins are the template.

## Phases (each gated by the FULL battery + warn_sweep_cxx20, both SDK legs, UNIFORMITY_STRICT=1)

- **P0 — constants at 16 (no-op commit):** introduce `MAX_STEPS`/`FRAME_MASK` constants bound to 16;
  retype nothing; battery green bit-identically. Makes P1 a diff of values, not of structure.
- **P1 — the storage frame:** arrays (A), return types, views. Compile + battery (goldens hold).
- **P2 — the mask planes:** uint32_t family, rotMask32, windowMaskBits 32, editor hi/lo rows (B).
- **P3 — the plumbing:** configParam ranges, updateWindow clamps + CV per D1, LOR_LEN range (D).
- **P4 — the literal sweep:** every & 0x0F / %16 site per the C table (frame → 0x1F; musical → stays;
  voice → untouched). Grep-verified zero unclassified literals at close.
- **P5 — the surfaces:** ring SVGs (32 lamps, enlarged, position-only), STEP_LIGHTS enum + widget bind,
  Sands stepW, Lantern N_STEPS (F). Anchor audit 9/9.
- **P6 — pins:** new suite test_steps_32: len-32 march vs the closed forms (all four shapes — 32 is a
  power of two, the congruence laws' easy case); warm 16↔32 mid-run transitions == f(t, config);
  D3 old-vs-new window pins (offset 15 + len 16, documented); legacy-json load (16-wide keys → [0..15],
  play identical); mask hi/lo roundtrip; ring anchor count. Digest re-capture list per THE GOLDEN LAW.

## Decision points (for Rodney)

- **D1 — CV formulas:** extend proportionally (*31 — recommended: a window CV that cannot reach
  steps 17–32 defeats the feature; patch CVs re-calibrate, release-noted) vs preserve *15 (CV caps at
  16 — bit-identical CV behavior, functional hole). Digest sites affected by the extension: the
  CV-driven window pins in test_step_advance.
- **D2 — defaults:** stay 16 / 0 (recommended; fresh + existing patches unchanged) vs 32 (fresh patches
  suddenly play 32 — rejected).
- **D3 — window topology:** accept the extension semantics at offset+len > 16 (recommended; preserving
  the 16-wrap is a 16-mode in disguise) — the one ≤16 behavioral shift.
- **D4 — mask rows:** hi/lo float pair (recommended; ≤16 rows bit-identical, no precision wall) vs
  double-typed rows (json doubles hold 2³²−1 exactly, but retypes the editor plumbing).
- **D5 — the A/B gate:** the musical listen-test (STEP_COUNT_DECISION's audit) gates the RELEASE, not
  the branch — with one length knob the A/B is a knob turn on the same build.

## Sequencing

Own branch off the lane tip AFTER feat/position-holders lands (never ride the same train — this touches
the copula/CA/reversibility/hot-path frame the position-holder work just stabilized). P0 lands first as
the reviewable no-op; the resize commit is P1–P4; surfaces and pins follow. The battery is the safety
net at every gate, same discipline as the position-holder refactor.
