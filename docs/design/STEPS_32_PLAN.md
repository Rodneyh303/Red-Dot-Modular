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
2. **MUSICAL 16 — the bar of 16ths** (the clock's native step = one 16th — ClockEngine::pulsesPer16th).
   **INVARIANT in the DSP.** The only 16s in this class are PRESENTATIONAL: the ring's red quarter
   rule (`i % 4`, MonsoonWidget.cpp:474–475) and Lantern's bar grouping (:1118). At 32 steps the
   pattern is two bars of 16ths — which is the SAME ring read as one bar of 32nd notes (Rodney's
   resolution note, → D6): the reading decides only where the bar line and anchors are DRAWN, never
   how anything wraps.
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
reseat :249); the backward/forward walk bounds :475–488; `voiceRead_` :697 (`wnow` is own-length-bounded,
so L+O ≤ 31 < 32 — the widening is unobservable below D3);
the snap path :779–792 (incl. the 16-bounded window search :786); `updateWindow` offset mask :800/:806–808;
rel-light/trail arithmetic :849/:850/:880/:898; `refreshPolyRandomCell` :1653–1660; :1769/:2175/:2213.
PatternEngine.cpp:256 (warped_ cell write) and PatternEngine.hpp:545 (warpRow read) — `step & 0x0F`.
SequencerEngine.hpp:596/:601/:608 (phrase-delta helpers, mod-16 with +16 bias).
Monsoon.cpp:1148 `phaseStep = (absStep % 16 + 16) % 16` — **FRAME 16, extends** (the drift-snap:
the phase-derived step lands in the frame to compare against `engine.stepIndex` — misclassified as
musical in the first draft of this doc; it is the frame landing of the transport phase, and both
sides of the comparison must land in the SAME frame, so `% 16` → `% MAX_STEPS`).
StraitsSandsMacroVisual.cpp:1280/:1306 `calcPlayhead(...) & 0x0F` — frame value → 0x1F.

**The PHASE PATH (D9's audit find — PhaseEngine was NOT in the first inventory):** the SPAN
PhaseEngine.hpp:79 `PPB = p16 * 16` (96 positions @ 24 PPQN = 16 sixteenths = one bar) →
`p16 * MAX_STEPS` (192 = 2 bars @ 24); the TWO landings SequencerEngine.cpp:779 (the jump snap) and
Monsoon.cpp:1148 (the drift-check) `% 16` → `% MAX_STEPS` IN LOCKSTEP — the drift-corrector
:1145–1151 compares against the stepIndex :779 sets, so asymmetric landings snap-fight every sample —
the lockstep is SPAN-ORTHOGONAL (D12: both landings are identical readers of the same `pulsePos`; the
fight is asymmetric EDITING, not the span — what the span decides is whether the landing's domain can
name the walk's at all);
the window scan :790 (`s < 16 ... & 0x0F` → MAX_STEPS / 0x1F); the jump-replay within-draw cap
Monsoon.cpp:1103 `if (steps > 16) steps = 16` → 32 (a FRAME value — "at most one boundary", :1099);
the derived BPM PhaseEngine.hpp:105 `barsPerSec * 4.0 * 60.0` assumes 1 phase unit = 1 bar = 4 beats
→ `* 8.0` (else the bpm readout and `sixteenthSec` :108 HALVE). STAYS MUSICAL: :165–166 the bar-local
`step16 % 16` + `step16 % 4 == 0` quarter mark (mod-4 of the absolute step is INVARIANT under the
%16 landing since 16 ≡ 0 mod 4) — it survives the span extension verbatim and IS D6(a)'s ring marking.
Full reasoning in D9. The tier extension (D13) adds the composite helper and its three readers as NEW
phase sites — sequenced P7; the sites above are the STANDALONE map and are unchanged by it.

**THE TWO PASS-TRACKING EXCEPTIONS (D7 — NOT frame sweeps):** `macroFrameRead_ = (tNow − 1) % 16` :643
and the identity-row `voiceRead_ = (tNowV − 1) % 16` :681 consume the RAW driver (pass-through at the
default window), so their `% 16` is the ACTIVE PASS, not the frame — today pass == frame == 16 and the
ring mod is accidental-correct. At (16, 0)-on-32 a naive `% 32` sweep lands cell 16 while step 0 plays:
the macro/identity data reads desync from the playhead on pass 2. The invariant: the landing EQUALS the
played step (both feed data reads). → mod the driver's own cycle — the pass (16) at the pass-through
default; at mapped windows the driver is bounded so any modulus ≥ len is identity (`% MAX_STEPS` correct
there). The only two exceptions to the frame sweep; grep-verified at P4.

**The one genuine ≤16 semantic shift (D3):** the window WRAPS the frame today — offset 15 + length 16
plays the rotated full ring [15,0,…,14] (endStep `(15+16−1) & 0x0F` = 14). At frame 32 the same config
extends instead ([15…30]) — "length 16 = the whole ring" becomes "length 32 = the whole ring". This is
inherent to growing the ring (the rotated-order semantics survive; the full-ring-at-16 identity does
not). Preserving the 16-wrap would reintroduce frame-16 logic — a 16-mode in disguise. Accept + pin
old-vs-new in the new suite.

## The boundary vs the ring (the wrap regimes, Rodney-approved formulation)

Every wrap in the tree has TWO independent parts:

- **The BOUNDARY — what length triggers the wrap — is always a LIVE parameter.** The pattern
  playhead wraps at the master window's length/offset (`wrapped = (stepIndex == startStep/endStep)`,
  SequencerEngine.cpp:483/:491, membership via the live `windowMask`); each voice's LOR walk wraps at
  ITS OWN length (`wnow = (laneTickFor(vd, tNowV, L) + rho) mod L`, :694–695 — NOT the master's);
  lane walks at their own `strandLen`/`macroLOR_` (:451/:464). The transport clock `t` wraps at
  NOTHING (unbounded since the DNA_LCM kill). The master length/offset does NOT cascade into lane
  wraps — a voice at LOR len 5 wraps every 5 inside an 11-step master window (the nested beating).
  What the master window DOES reach is the lane PHASE (Rodney's "does master offset offset the
  probability reads?" — answered from fresh reads): the walks are driven by the driver map
  (`driverTickAt` :95–100 — pass-through at the default window 16/0, otherwise the 1-based walk
  through the window; called for the mono lanes :417 and PER VOICE :663, the advance offsets riding
  inside), so a default lane (len 16, off 0, rot 0, Forward) reads exactly the windowed cells
  IDENTITY-MAPPED — master step s still draws cell s (window [3..12] → the lane draws cells 3..12;
  cells 0–2/13–15 go silent for default lanes while the window is authored), and the wrap re-lands
  on the window start's tick each pass. Shorter lanes keep their OWN wrap (`laneTickFor` mods by
  the lane length :48–54) — the master window re-phases their polymeter beat (against the window
  walk, not the 16-count) but adds nothing to their offset. The master offset NEVER enters
  `getStrandIdx` (:2168–2176): the only offset that slides the probability DATA is the LANE'S OWN
  LOR off. Thresholds (variationProb/restProb/…) and the pairing inside the window are untouched.
  The driver's `% len` IS the master length — at 32 it extends with the window range (D3's layer);
  the final landing stays `% 16` until the ring sweep.
- **The RING — the frame the arithmetic lands on — is a CONSTANT.** Every `& 0x0F` / `% 16` landing
  between boundary checks (the playhead walk :476/:486, `voiceRead_ = (wnow + O) & 0x0F` :697 — the
  offset rides, the landing is the frame; the phrase helpers hpp:596–608), the window-mask build
  (setWindow :236–251), the A6 full-phrase constant (`voiceFrameReadsFullPhrase → L = 16` :690 — a
  frame value, not a parameter). ONE EXCEPTION (D7): `macroFrameRead_` :643 and the identity-row
  reads :681 land on the ACTIVE PASS, not the frame — they consume the RAW driver, so their modulus
  tracks the playhead's cycle, not the ring.

At 32 the boundary layer extends by its EXISTING parameters (window 1..32, LOR len 1..32 — the
position-holder walks already take len > 16) and the ring becomes 32 in one mechanical sweep. The
layers never entangle except at the two documented sites: D3 (a live window that wraps the 16-ring
today extends instead at frame 32) and the A6 constant (→ MAX_STEPS).

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

- **Ring (as amended by D15 — the 16-stay form):** embed_monsoon.py:32–33 — the 16 `STEP_LIGHT_{i}`
  anchors at r=14 STAY (no enlargement, no 32 anchors; twice-around-16 per D15 — lamp k carries
  masters k and k+16, the current lap from the playhead's half, D10's split test) + TWO new BAR
  1/BAR 2 anchors INSIDE the ring's unused center (D15's position pin; position-only, no
  pattern-detail role, red-quarter/green-rest kept). Monsoon.hpp:500–503 `STEP_LIGHTS_END =
  START + 16` STAYS; the BAR LightIds append after it (offset-based enum re-seats the downstream
  ids automatically). MonsoonWidget.cpp:474–475 bind loop stays 16 + the two BAR binds; the
  `i % 4` red rule is MUSICAL (quarters at the clock's 16th resolution) — under twice-around the
  anchors mark the CURRENT lap's quarters (D6(a) verbatim on the rendered lap; the two bars'
  quarters coincide on the same four lamps — the D15 accepted loss).
- **Ring feed (as amended by D15 — stays 16-wide):** Monsoon.cpp:1461
  `float stepBrightness[16]` + :1462 the fill loop + :1492 `updateStepLights(stepBrightness, 16)`;
  MonsoonUIManager.cpp:100 `for (int i = 0; i < 16 && i < count; ++i)` — ALL STAY 16-WIDE (the
  display is the twice-around ring, not a 32-lamp frame; nothing here → MAX_STEPS). The playhead
  comparison (`lightIdx == stepIndex`, SequencerEngine.cpp:874) becomes
  `lightIdx == stepIndex % 16` — the ONE sanctioned display-path `% 16` (the exact landing class
  P4 sweeps out; D15 accepts it HERE and only here: display feed only, no engine read, no digest —
  THE GOLDEN LAW holds). The window/ghost test reads the CURRENT-LAP master
  (`isStepInWindow((stepIndex >= 16 ? 16 : 0) + lightIdx)` — the lap is D10's split test, shared
  with the BAR LEDs; no second lap source exists). The trail's existing `% 16` (:880) is untouched.
- **Sands grid:** StraitsSandsMacroVisual.cpp:388/:460 `stepW = (ED_W − 12)/16` → `/32` (cells halve;
  the accepted HP cost). Widget-drawn = no SVG regen for the grid itself.
- **Lantern:** Lantern.cpp:499 `N_STEPS = 16` → 32 (nvg-drawn, no assets); :1118 bar grouping
  (barW = grid/4) is presentational — per D6: unchanged under (a), re-grouped under (b).
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
- **THE GOLDEN LAW:** every existing digest holds bit-identically at len ≤ 16 AND offset+len ≤ 16 —
  for EVERY (off, len) pair: the master window AND each lane/voice LOR (a read reproduces exactly when
  it never crosses cell 15, making the % 16 → % 32 landing widening unobservable).
  The two documented shift classes — D1 CV formulas (if extended) and D3 window-topology at
  offset+len > 16 — are baseline re-captures, enumerated site-by-site with one-line reasons, house
  precedent: the subgate draw-source fix ("digests shift (baseline, not regression) — re-capture on
  the fixed base").
- **THE REPRODUCTION LAW (Rodney: "this should simplify reproduction 16 step equivalent on 32 step
  grid"):** the 16-equivalent on the 32 grid is a CONFIGURATION, not a mode — the default window
  (16, 0) IS the 16 engine on the 32 frame. The driver pass-through (`driverTickAt` :96) keeps every
  lane walk's raw congruence (a len-7 lane keeps its 7-cycle — no re-phase), the windowed playhead
  fires steps 0..15 and wraps, and every read lands in cells 0..15 — the widened ring is never
  observed. No shim, no dual path, no mode bit (the MODE LAW's strongest form). The reproduction set:
  every (off, len) pair with off+len ≤ 16 (THE GOLDEN LAW's condition, restated positively); legacy
  json patches fill [0..15] through the 16-bounded loaders and reproduce as-is. The non-reproducing
  set is exactly the D3 wrap class — plus nothing.
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
- **P5 — the surfaces (as amended by D15):** ring SVGs (the 16 lamps at r=14 STAY — no enlargement,
  no 32 anchors; twice-around per D15) + the two BAR anchors INSIDE the ring center (both themes),
  STEP_LIGHTS enum + widget bind + the ring feed (the F find, 16-wide), Sands stepW, Lantern
  N_STEPS (F). Anchor audit 9/9 (re-based on the 16+2 anchor set). PLUS D10-as-amended-by-D15: the
  window map (the ghost arc — the one else-branch in getStepLightBrightness, the CURRENT-LAP test
  per the F find, ghost pinned below the in-window rest) + the BAR 1/BAR 2 LEDs (SVG anchors both
  themes; LightIds appended after STEP_LIGHTS_END, re-seat automatic).
- **P6 — pins:** new suite test_steps_32: len-32 march vs the closed forms (all four shapes — 32 is a
  power of two, the congruence laws' easy case); warm 16↔32 mid-run transitions == f(t, config);
  D3 old-vs-new window pins (offset 15 + len 16, documented); legacy-json load (16-wide keys → [0..15],
  play identical); mask hi/lo roundtrip; ring anchor count (16 STEP_LIGHT anchors + 2 BAR anchors
  inside the ring center, per D15; `i % 4` red on the CURRENT lap under D6(a));
  phase-span pins (0–10 V ↔ steps 0..31 identity, reconnect/reset lands on the SAME step — the D9
  reset-safety invariant; drift-check quiescence during parked arcs); ring-map pins (D10 as
  re-scoped by D15: ghost strictly below the in-window rest at (16,0) AND at a D3 wrap window; the
  CURRENT-LAP re-scope is ATOMIC — indicator, dim set and playhead lamp move together within one
  UI frame at the tick; one-bar windows render STABLE — no flip, the D15 claim, script-verified
  (scripts/d15_ring_flip_table.py); straddling windows flip once per pass at the playhead's tick
  crossing (the designed tell); the full window renders no ghosts and the BAR LEDs alone carry the
  lap (the P6 pin transfers); BAR LEDs exclusive, follow stepIndex, a window change re-renders
  within one UI frame; both silent wherever the discrete playhead is absent — mode D).
  Digest re-capture list per THE GOLDEN LAW.
- **P7 — the phase tier (D13, post-32):** the composite helper (ONE function; its readers: the reseed
  :86–92 + the two landings :779/:1148), the CV2 subphase input (the mode-E takeover per the
  :1714–1718 mode-gate precedent), the regionSize calibration knob (detents {1, 2, 4, 8, 16}
  sixteenths, default bar), bpm-from-the-finest-ramp, jump-path reuse (cap 32, the tier-domain
  clamp). Pins: standalone-192 bit-identical with CV2 unpatched; composite injectivity across the
  detents; per-tier reseed exactness (reconnect mid-bar-2 lands exactly — the D9 invariant per tier);
  zero-skew region-boundary quiescence (no spurious jumps when the tiers are locked); plug/unplug
  transitions land through the jump path; the aliasing-trap release note (a 1-bar source on the
  transport jack at N = 2 aliases — bar-1-first/bar-2-second, half the pattern, silent). Digest
  re-capture list per THE GOLDEN LAW (the composite path enters the phase-span pins). Idea 3's
  distribution transform: USER-ENDORSED at D16 — sequenced after the tier build (post-P7), the
  Sands/East extraction refactor the prerequisite, the Golden-Law digest gate unchanged.

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
- **D6 — anchor marking at 32 (from Rodney's resolution note: "32 steps is 1 bar at 32nd notes") —
  DECIDED (Rodney: "red quarters every 4 + the bar tick at 16 — Agreed"):**
  the ring is resolution-agnostic — 2 bars of 16ths (the clock's native step) and 1 bar of 32nds are
  the SAME 32-step frame; no DSP site depends on the reading. The reading decides presentational
  marking only: **(a) CHOSEN** — honest-to-clock: steps ARE 16ths, red quarters every 4 steps (8 red
  anchors) + a bar tick at 16 (the anchors keep marking the clock's actual quarters; the `i % 4`
  widget rule survives; the two-bar reading matches how the long-note gain was argued
  in STEP_COUNT_DECISION — whole notes fit twice). **(b) declined** — 1-bar-of-32nds: red every 8, no
  mid-ring bar tick. Zero DSP impact either way.
- **D7 — the pass-through pin at 32 (from the reproduction audit):** `driverTickAt`'s pass-through
  condition (`len == 16 && off == 0`, :96) stays LITERAL — it means "the 16-default window", not "the
  frame default". Re-keying it to the frame (mapping (16,0)-on-32) re-phases EVERY non-divisible lane
  (a len-7 lane's 7-cycle becomes a 112-beat; triangles lose their raw beat) — a reproduction break
  far beyond the edge cases; rejected. Consequence: the two pass-tracking landings :643/:681 mod the
  ACTIVE PASS (the C-table exceptions above) — the only sites where "sweep the frame" is wrong.
- **D8 — the ring count (Rodney: "Should we replace the 16 step circular ring with a 32 step one or
  accept that 32 steps is 2 bars so twice around 16 steps? Probably 32 better as Master steps are
  directly indicated in it"):** REPLACE — 32 lamps on a FIXED master-step scale; twice-around-16
  rejected on five counts. **(1) Injectivity:** on 16 lamps lamp = step % 16 — steps s and s+16
  share a lamp, and any walk with off+len > 16 (the D3 class) folds the window onto itself; on 32,
  lamp s IS master step s (the direct indication Rodney wants), and the live window reads as an
  ARC on the constant scale (start lamp `off`, span `len`; the playhead's jump back IS the
  boundary — the boundary-vs-ring formulation made visible). **(2) No new % 16:** the playhead
  compare is `lightIdx == stepIndex` (:874) — frame-generic as written; twice-around REQUIRES
  `stepIndex % 16` in the display path, a new frame-16 landing exactly where P4 sweeps them out.
  **(3) Pass-blind:** D7 made the ACTIVE PASS load-bearing (:643/:681); a twice-around ring cannot
  distinguish pass 1 from pass 2 and the bar line is undrawable (lamp 0 fires both passes); at 32,
  pass 2 is the far half and D6(a)'s mid-ring tick at 16 exists. **(4) The 2-bar reading is
  MARKED, not lost:** the ring is resolution-agnostic (D6) — 2 bars of 16ths = 1 bar of 32nds =
  the same 32-frame, the reading decides MARKING only; D6(a) gives 8 red quarter anchors (the
  `i % 4` rule survives VERBATIM at 32) + the bar tick, while twice-around collapses both bars'
  quarters onto the same 4 lamps. **(5) Cost is a wash:** P5 regenerates the SVGs either way
  (anchors are generator-emitted; the legibility lever is RADIUS — circumference scales with
  radius, not lamp count); twice-around leaves Monsoon the ONLY surface at 16 beside Sands
  stepW/32 + Lantern N_STEPS=32 — a special case against the EXTENSION LAW. This DECIDES
  LIGHT_RING_32_STEPS's OPEN question too: FIXED 32 — "master steps directly indicated" needs a
  constant angular address per master step (adapt-to-length re-spaces; step 8's lamp would move on
  every knob turn). At the default (16, 0) the far half stays dark — honest: the playhead never
  enters it (the REPRODUCTION LAW made visible on the panel).
- **D9 — the phase-input span (Rodney: "Phase mode which is mode c. Phase 0 to 10v is quantised to 96
  steps based on 24 ppqn for 4 quarter notes. Similar issue to light ring. Leave this and Phase 0 to 10
  represents 1 bar. Or quantise to 192 steps and one full Phase cycle represents 2 bars at 24ppqn, or 1
  at 48ppqn."):** QUANTISE TO 192 — the span extends to the frame (`PhaseEngine.hpp:79`
  `PPB = p16 * 16` → `p16 * MAX_STEPS`: 0–10 V = 2 bars @ 24 PPQN = the full 32-step cycle). The "1
  bar at 48 PPQN" reading is VOID — PPQN_48_96's DECISION requires 24 (48/96 nominal/unsafe,
  near-zero). The deciding structure is D8's, one level deeper — **THE PHASE INPUT IS RESET-SAFE IFF
  SPAN == FRAME.** `normPhase` wraps the voltage into [0,1) and `prevContPhase` accumulates
  shortest-path deltas (:97–99), so `pulsePos` is the UNWRAPPED absolute pulse and
  `absStep = pulsePos / p16` carries the revolution count k (absStep = bar-local + k·span). On
  disconnect/reconnect or reset (`havePrev=false` :81, `reset()` :55–60) pulsePos RE-SEEDS from the
  bar-local position (:86/:91) — k is LOST. At span == frame the landing (bar-local + k·frame) mod
  frame = bar-local — k invisible, a reconnect lands on the SAME step (today's span-16-on-frame-16 is
  safe the same way D7's :643/:681 are accidental-correct). At span 16 on frame 32: odd k shifts the
  landing +16 — reconnecting the phase cable on an odd pass plays the OTHER half of the pattern. Pass
  parity becomes LOAD-BEARING, carried by an accumulator that silently resets — D8's twice-around
  rejection in its sharpest form (there the pass was merely invisible; here it is load-bearing AND
  volatile). Span 32 restores the invariant (k·32 mod 32 = 0) and makes the voltage INJECTIVE:
  voltage ↔ master step one-to-one — one sweep = one full pattern = one full ring revolution (the
  phase input and the D8 ring AGREE). **COMPATIBILITY IS D1-CLASS, NOT FREE:** the phase span is
  FRAME-domain, NOT window-relative — today the phase lands IDENTITY on the frame and out-of-window
  landings PARK at the window start (:788–792; the driver map's window walk is the LANE layer, not
  the input layer) — so at the default (16, 0) the second half of a 0–10 V sweep parks instead of
  wrapping: every existing mode-C patch re-calibrates. The argument is D1's own ("a window CV that
  cannot reach steps 17–32 defeats the feature"): a phase input that cannot reach steps 17–32
  defeats the feature. Release-noted; CV pins re-capture. The window-relative alternative (0–10 V =
  the window arc, start at off) is REJECTED per D8's own logic: the voltage loses its constant
  address (the same V means a different step per knob turn) and entangles the input with the live
  boundary layer — a NEW semantic, not a preservation (today is frame-absolute). WHAT IS INVARIANT:
  the musical 16 inside the phase engine — the quarter mark (:165–166) — survives verbatim and IS
  D6(a)'s ring marking (the phase's quarter edges = the red anchors; the bar tick at 16 = the
  mid-ring tick). THE SITES are in the C table's phase-path find above. P6 pin added: drift-check
  quiescence during parked arcs (the corrector :1145–1151 re-fires per sample while the landing
  parks out-of-window — pre-existing today at mapped windows; wider arc at span 32).
  **CLARIFIED (Rodney: "OK and phase mode phase represents 1 bar or 2? it's just mapping phase to
  clock"):** TWO BARS — and "just a mapping" is exactly the right frame: 0–10 V is a POSITION MAP
  onto the clock's own step domain (same steps, same quarter anchors, same ring — not a second
  time domain). The span == frame rule is what MAKES it a plain mapping: at span 16 the voltage
  would address a domain half the clock's size — two clock positions per voltage, revolution
  parity k deciding, k volatile on reconnect (the reset hazard above); at span 192 the map is 1:1
  — one sweep = one pattern = one ring revolution. THE CONCRETE MAP (P6 pin constants): 24 PPQN →
  192 pulse positions per sweep → **0.3125 V per 16th (= per step), 1.25 V per quarter (the red
  anchors), 5.000 V at the bar tick (the step-16/17 boundary — in the stepped phase modes this
  crossing IS the BAR 1/BAR 2 swap, D10), 10 V wraps to step 1**. The two places it is MORE than a
  mapping, both already pinned: (a) the voltage addresses the FRAME, not the window —
  out-of-window landings PARK at the window start (:788–792; the D1-class re-calibration,
  release-noted); (b) the module derives TIMING from the ramp — the bpm readout
  `barsPerSec*4.0*60.0` → `*8.0` and `sixteenthSec` :108 (the C-table find): the one constant
  that must change, because a cycle is now 2 bars.

- **D10 — the ring's window map + the bar indicator (Rodney: "we could have a 1st and 2nd bar
  indicator sync with the light ring. If only a subset of the 1st 16 steps are included in the master
  range the others can be dimmed during bar 1 indicator and same for the 2nd 32 steps are bar 2
  indicator so the light ring would change dynamically."):** ACCEPTED — DISPLAY-ONLY, both halves.
  (Reading: "the 2nd 32 steps" = the 2nd 16, steps 17–32; "dimmed during bar N indicator" = a
  STANDING map — a lamp is dim iff OUT of the master range, in either bar, not only while that bar's
  indicator is lit.) Two additions, both DERIVED — no new engine state, no new landing, no CV, no
  persistence key.
  **(1) THE WINDOW MAP (the ghost arc):** out-of-window lamps light a dim ghost instead of today's
  dead 0.0. The membership predicate is the one the engine ALREADY maintains — `isStepInWindow`
  (SequencerEngine.cpp:232, the `windowMask` bit test the comet trail :881 already consumes), and the
  display mods it rides are already in the C sweep (rel-light/trail :849/:850/:880 — the C table's
  :86 line). The change is ONE `else` branch in `getStepLightBrightness` (:843): the four existing
  levels keep their values VERBATIM — playhead 1.0 (:874) > comet trail 0.5 (:882) > in-window note
  0.35 > in-window rest 0.07 (:866) — and the new ghost pins STRICTLY BELOW the in-window rest, so a
  ghost can never read as a rest (the exact value is a P5 panel-calibration lever, same class as
  D8's radius; the HIERARCHY is the law). THE GOLDEN LAW untouched: the digests never capture the
  display path (`getStepLightBrightness` is in no test), and 0.0 → ghost changes no engine read. The
  map is LIVE/post-CV for free: `windowMask` is rebuilt by `setWindow` (:236–251) from
  `updateWindow`'s post-CV values (:797–809), and the ring feed re-renders every UI frame
  (Monsoon.cpp:1461/:1492 → MonsoonUIManager.cpp:100) — "the light ring would change dynamically"
  falls out of the existing plumbing; knob, CV and D3-wrap windows all render the same way. This
  AMENDS D8's closing sentence: at the default (16, 0) the far half is no longer dead-dark — it is
  the ghost arc; the honest part holds (the playhead never enters it — the REPRODUCTION LAW visible
  as the content/ghost boundary).
  **(2) THE BAR INDICATOR (BAR 1 / BAR 2):** two LEDs at D6(a)'s mid-ring tick (position = P5
  calibration), lit EXCLUSIVELY — BAR 1 while the playhead is in steps 0–15 (master 1–16), BAR 2 in
  16–31 (master 17–32). The split test is `stepIndex >= 16` — D6(a)'s own presentational bar-tick
  domain, NOT a new % 16 landing (stepIndex is the already-landed frame value; the compare wraps
  nothing, it READS the bar layer). The sync is LITERAL: the LEDs share the playhead's exact gate
  and variable (:873–874 — the same stepIndex the ring's current-step lamp consumes; one gate, two
  surfaces — if the :869–872 comment's E/F intent restores a phase-mode playhead, the bar LEDs come
  along automatically; the sync must never fork). Where no discrete playhead exists (mode D),
  neither LED lights. New LightIds ride P5's offset-enum re-seat (append after STEP_LIGHTS_END,
  Monsoon.hpp:500–503; downstream ids shift automatically).
  **WHY THIS CLOSES THE 16-RING CHALLENGE (Rodney: "intuitively I'd argue for the 16 steps ring and
  single bar phase as they seem more natural. Are the challenges that hard? … apart from length and
  offset not being visible on 16 step ring are the objections that strong/challenging?"):** the
  objections were never engineering difficulty — they are ADDRESS ALIASES, and this proposal is
  their reductio. On twice-around-16, lamp = step % 16: lamp 5 is steps 5 AND 21, so a window 1–20
  makes lamp 5 in-window as 5 and out-of-window as 21 — "dim the steps not in the master range" is
  UNDEFINED (one lamp, two verdicts), and a 1st/2nd bar indicator would have to read a pass parity
  the panel cannot observe (D8's pass-blindness). The first display feature reached for past the
  basics is unimplementable at 16 — D10 is not just compatible with D8, it is EVIDENCE for it. The
  single-bar-phase half of the challenge is the same alias one level deeper and is a CORRECTNESS
  hazard, not a preference (D9: span 16 on frame 32 makes revolution parity k load-bearing AND
  volatile — an odd k after a reconnect lands +16, the other half of the pattern; span 32 kills k
  and makes the voltage injective). "Natural" was tracking the musical bar; the frame is the
  pattern (2 bars); D10 is the repair in kind — instead of displays that under-report the frame,
  SHOW the frame: the ghost arc for the window, the mid-ring tick for the bar line, the exclusive
  LEDs for the live bar.
- **D11 — the alternating lap-dim (Rodney: "what I am suggesting 1st lap around 16 (a new visual
  indicator to tell 1 or 2) lamps 1-4 dimmed, 2nd lap 21-32 dimmed, 3rd lap 1-4 dimmed, 4th lap
  21-32 dimmed"):** DECLINED as an alternation; D10's STANDING map + BAR LEDs stand. The reading: a
  twice-around-16 ring whose dimmed region alternates with the lap — lap 1 ghosts the lamps carrying
  lap-1's out-of-window masters (1–4), lap 2 ghosts lap-2's (21–32 → lamps 5–16), laps 3–4 repeat
  (period 2 laps = one 32-step pattern); the numbers are exactly a 5–20-class window's exclusions
  (bar-1 head + bar-2 tail), and "(a new visual indicator to tell 1 or 2)" is the lap tell. **THE
  CREDIT, RECORDED FIRST:** ported to twice-around-16, the D10 ghost map is LAP-CONDITIONAL for free
  (a lamp's current-lap master differs per lap — lamp 3 is master 3 = OUT on lap 1, master 19 = IN
  on lap 2), which makes the PASS OBSERVABLE: D8's count (3) pass-blindness is repaired BY the map,
  and with the lap indicator the lamp⇄master map is conditionally injective. This is the best
  twice-around salvage on the table — and it still loses, on three counts extending D8's list.
  **(6) THE TELL IS INVERSE TO WINDOW SIZE:** the lap tell IS the ghost and the ghost IS the
  exclusion — at the full window (1, 32) there is no exclusion on either lap, no ghost anywhere,
  the tell is GONE, and the ring is pass-blind exactly when the 32-step feature is fully used; the
  tell decays continuously as the window grows (window 1–31 leaves ONE ghost lamp on lap 2 — master
  identity hanging on a single lamp's brightness step). A display law that fails at the feature's
  maximum is not a display law: the ring's address space must not depend on window shape. **(7)
  IDENTITY BY FUSION:** every lamp read becomes (lamp, lap-indicator) — the "master steps directly
  indicated" property (Rodney's own D8 rationale) is retracted — and the ghost level becomes
  LOAD-BEARING for identity: ghost-vs-rest (0.07) must stay reliably readable or the lap read
  fails, but the D10 hierarchy was calibrated as decoration, not as address bits. **(8) THE WINDOW
  READS AS TWO DISJOINT ARCS AND THE BARS OVERLAY:** on 16 lamps the window 5–20 occupies lamps
  5–16 plus 1–4 — a broken shape, parseable only WITH the lap bit — and the two bars' patterns
  share the same lamps one lap at a time: the 2-bar comparison, the main question a 2-bar pattern
  asks, is unseeable. (Cost unchanged, D8 count 5: P5 regenerates either way; radius is the
  legibility lever.) **ON THE 32-RING the alternation would LIE:** at frame 32 out-of-window is
  NOT lap-conditional (lamp 3's master is 3, every lap) — 1–4 AND 21–32 are excluded on every lap,
  so D10's standing map already shows "1–4 dimmed" during lap 1 AND "21–32 dimmed" during lap 2:
  BOTH of Rodney's observations are true of the standing map on EVERY lap. Alternating the ghost
  would present the un-dimmed region as playable half the time — misstating the master range, the
  one thing the map exists to state. **THE PER-BAR ASYMMETRY IS REAL AND ALREADY DISPLAYED:** it
  lives in the WINDOW WALK, not the map (bar 1 fires 5–16, bar 2 fires 17–20, wrap jumps back to
  the start — the walk crosses the tick by construction, SequencerEngine.cpp:486/:491); the
  playhead's sweep relocates each bar inside the standing content arc, the BAR 1/BAR 2 LEDs swap at
  the tick (bit 4 of stepIndex — the lap tell, D10), and laps 3/4 are the pattern's second cycle:
  the transport is unbounded (t wraps at NOTHING since the DNA_LCM kill) and no cycle-parity state
  exists, so the indicator only ever needs to say 1 or 2 — which is BAR 1/BAR 2. P6 pin added:
  full-window (1, 32) — the ghost map is empty there and the BAR LEDs alone carry the bar tell
  (the case the 16-lap scheme cannot render; pin the ring reads correctly at the maximum).
- **D12 — the 1-bar phase proposal (Rodney: "Phase — 1-bar wrap: the challenge is real but NOT that
  hard. The doc's concern is the “snap-fight every sample” — the jump-snap (:779) and the drift-check
  (Monsoon:1148) must land in the same frame, or they fight. But that's true regardless of 1-bar or
  2-bar — it's just “keep the two landings consistent.” The actual question is: does phase span 1 bar
  (16 steps) or 2 bars (32 steps)?

  1-bar phase (my intuition): phase quantizes over 16 steps, and 32-step patterns have phase that
  repeats each bar. Simpler, matches the “phase = bar position” mental model. The PPB = p16 * 16 stays
  as-is (96 = 1 bar). Minimal change — phase behaviour is bar-local, unchanged from 16-step.
  2-bar phase (the doc): phase spans the whole 32-step pattern, PPB → p16 * 32 (192), BPM *4 → *8,
  both landings % MAX_STEPS. More change, and the “snap-fight” risk if the landings aren't lockstep.

  So 1-bar phase is actually the EASIER and more natural choice — phase stays bar-local (96,
  unchanged), you don't touch PPB/BPM/the landings, and “phase = where in the bar” is the intuitive
  model. The doc's 2-bar version is more work (extend the span, double BPM multiplier, lockstep the
  landings) for the arguably-less-natural “phase spans the whole 2-bar pattern.” my intuition (1-bar
  phase) is both more natural AND less code."):** DECLINED — the 2-bar span stands (D9 + its CLARIFIED map, unchanged); the proposal's two
  verifiable claims are CONCEDED and pinned first, and the verdict turns on the one claim that is not
  available to take. **THE CONCESSIONS, CODE-VERIFIED (recorded first):** (a) THE SNAP-FIGHT IS
  SPAN-ORTHOGONAL — both landings (SequencerEngine.cpp:778–779 jump snap, Monsoon.cpp:1147–1148
  drift-check) are textually identical readers of the SAME accumulator (`absStep = pulsePos / p16`),
  and the corrector :1149 compares against the stepIndex :786 sets — they can fight only if EDITED
  asymmetrically; the lockstep is a standing invariant of the PAIR under any span (the C-table clause
  is re-worded accordingly). (b) THE CONSTANT CHURN IS REAL BUT SMALL — at span 16 `PPB = p16 * 16`
  :79 stays 96 and `barsPerSec * 4.0 * 60.0` :105 stays ×4, exactly as proposed; the 2-bar plan's
  PPB→192 and ×8 are its true price, already pinned. **WHY IT STILL LOSES — "YOU DON'T TOUCH THE
  LANDINGS" IS NOT AVAILABLE UNDER ANY SPAN:** the window walk's domain is the WINDOW (up to 0..31,
  boundary-driven :483/:491) and the untouched landing's domain is 0..15 — at the first bar-2 crossing
  (pulse 96 → absStep 16 → `% 16` → 0) the every-sample corrector :1145–1151 re-lands the walk onto
  bar 1 (snap :1150 → :786), after which walk and landing agree BAR-LOCALLY: the module plays the
  FIRST BAR TWICE per 32-cycle, forever, silently, while the D10 ring displays the full window's
  content — a display/truth split. Windows fully inside the second half are worse: the landing cannot
  name their positions, parks at the window start every sample (:788–792), and the corrector fights
  the walk PER SAMPLE — the snap-fight made runtime, caused by the span, not by editing.
  "Unchanged from 16-step" holds exactly while the expansion is unused (at the default window (16, 0)
  the untouched landing agrees by construction — the walk never leaves 0..15). And the touched landing
  LINE IS IDENTICAL under both plans — `((absStep % 32) + 32) % 32` on `absStep = pulsePos / p16`
  (Monsoon.cpp:1128: "In Mode E the phase IS the transport"; the user's mode C) — so the decision is
  NOT code volume; it is **WHERE THE PARITY BIT LIVES.** At PPB 192, `absStep = 32m + c/6` and the
  m-term VANISHES mod 32: the landing is pure voltage, and the reseed (:86–91) IS the exact landing —
  `pulseInBar = floor(phase·192) ∈ 0..191 → /6 ∈ 0..31`; reconnect mid-bar-2 lands exactly (the
  pinned D9 invariant, now line-verified). At PPB 96, `absStep = 16k + b/6` and the high bits ARE the
  bar tell (`16k mod 32 = 16·(k mod 2)`) — correct only while k is right, and k lives in the unwrapped
  accumulator, which RE-SEEDS BAR-LOCAL on every reconnect (`connected=false → havePrev=false` :81 →
  `pulsePos = pulseInBar` :88–92) and on reset (:55–60): k = 0, bar 1. Re-plug mid-bar-2 → the module
  plays bar-1 content and the drift-check AGREES (both read the same seeded `pulsePos` — perfectly
  locked, silently wrong), and NOTHING in the signal can correct it: a 1-bar ramp carries ZERO bits
  of bar identity — information-theoretic, not engineering. Wrong until reset. **THE COST TABLE THIS
  BUYS:** the 2-bar plan's cost is the re-calibration — LOUD and ONCE (an old 1-bar ramp reads as a
  double-speed sweep, audibly glaring, fixed by re-scaling; release-noted, D1-class); the 1-bar plan's
  cost is a SILENT, PERMANENT wrong-half on the most routine operation in modular. Loud-once beats
  silent-permanent — and "less code" is here the MORE-STATE plan: 1-bar saves two constants and adds
  a load-bearing parity accumulator, its seed rule, and its failure class (a separate parity input —
  a bar/gate jack — would be a NEW contract, strictly more change than the constants). **THE MENTAL
  MODEL, ANSWERED ON ITS OWN TERMS:** the 2-bar bus is a SUPERSET address space, not a stranger to
  bars — a 16-frame consumer on a 2-bar bus lands correctly for free (`32m ≡ 0 mod 16`: the old
  `% 16` landing kills the m-term; the widening orphans nobody), while a 1-bar bus cannot address
  steps 17–32 at all (D1's argument verbatim). The bus span is a RIG-LEVEL agreement, like PPQN — the
  source needs the bus's span, not the consumer's pattern length: the rig's phase bus sweeps the
  longest pattern in the rig, and at 32 that is 2 bars. "Phase = where in the bar" survives INSIDE
  the map (0.3125 V per 16th, 1.25 V per quarter — the quarter spacing and the red anchors unchanged,
  :165–166 mod-4 invariant); what the wider sweep buys is that the second bar EXISTS on the wire. P6
  phase pins stand unchanged and are hereby line-verified (the reseed-at-192 IS the exact landing —
  the invariant that makes the accumulator's volatility harmless under the standing span).
- **D13 — the phase-tier counter-proposal (Rodney: "A better and richer approach is counter proposed.
  The 192 step becomes transport phase. On its own it acts like the document. With a 2nd sub phase
  input, the transport phase becomes more coarsely quantised and just tells us which sub phase we are
  in. Subphase determines where precisely.
  https://github.com/Rodneyh303/Red-Dot-Modular/blob/docs/steps-32-plan/docs/design/PHASE_TIER_SYSTEM_IDEA.md
  Then subphase can be any integer subdivision. We can have jumps within or across sub phase."; the
  document: docs/design/PHASE_TIER_SYSTEM_IDEA.md @ 97fa37fa — three ideas: (1) PHASE CALIBRATION
  (span a user parameter, default 1 bar, "supersedes D9's hardcoded 192"), (2) TRANSPORT PHASE +
  SUBPHASE quotient/remainder ("TRANSPORT PHASE ALONE = the 192 model (option A): the base — just the
  mapping"), (3) phase distortion through the distribution layer):** ACCEPTED IN PART — **Idea 2 is
  the extension path, sequenced P7 (post-32); Idea 1 is accepted ONLY as the tier calibration (the
  subphase regionSize knob) and DECLINED as a standalone-span override; Idea 3 + the
  distribution-primitive extraction are CAPTURED-DEFERRED, USER-ENDORSED at D16** (the document's own
  STATUS agrees: "sequence after 32 steps lands"; the D16 endorsement — "a great idea" — weights the
  backlog, the deferral and its gates stand). **THE CREDIT, RECORDED FIRST:** the tier split is the third
  option the D9/D12 binary missed — it satisfies D12's demand (bar identity carried by a VOLTAGE,
  never the accumulator) while restoring the existing 1-bar-ramp world through a second jack, and it
  restores the bar-bounded jump rule STRUCTURALLY at the fine tier. D12 is not overturned — its
  verdict was about the SINGLE-cable contract; the tier system adds a second cable and meets its
  demand in the new channel. **THE READING THAT HARMONIZES THE DOCUMENT'S OWN TENSION:** Idea 2's
  base IS D9 ("TRANSPORT PHASE ALONE = the 192 model (option A): the base — just the mapping") — the
  standalone contract stands UNCHANGED (subphase unpatched = the 192 map, bit-identical, the D9/D12
  pins intact); Idea 1's knob lives as the SUBPHASE calibration (regionSize); Idea 1 as a standalone
  span-override (default 1 bar, option B) is DECLINED — the document's own option-B description ("you
  must CLOCK THE CYCLES (count which bar), making it more like clock mode") is the D12 hazard
  verbatim: in mode E the phase IS the transport (Monsoon.cpp:1128), so the count's only carrier is
  the unwrapped accumulator, which RE-SEEDS BAR-LOCAL on every reconnect (:81 → :88–92) and on reset
  (:55–60) — correct at the default window, silently the wrong half once the window enters bar 2
  (D12's full finding). **THE COMPOSITE (the pinned mechanism):** position = quantise(transportPhase,
  regionSize) + subphase × regionSize — a PURE FUNCTION of the two instantaneous voltages; NO
  accumulator-carried identity at ANY tier; D9's invariant GENERALIZES: **every tier is reset-safe
  iff its voltage span equals its own domain** — the transport tier driven at the CALIBRATED
  TRANSPORT period, the subphase at the region period. The reseed (:86–92) and BOTH landings
  (:779/:1148) read the composite through ONE shared helper — the D12 lockstep now spans THREE
  readers, made structural (one function, not a discipline). **THE PER-TIER ALIASING TRAP (the
  document's missing warning):** a 1-bar-period source on the TRANSPORT jack at regionSize = 1 bar
  ALIASES — floor(norm × 2) flips MID-BAR (at norm 0.5), so the module plays bar-1's first half,
  bar-2's second half, alternately: half the pattern, deterministic garbage, silent. D12's zero-bits
  argument applies to EACH TIER verbatim: a tier's voltage must carry its own domain's identity. THE
  COMPATIBILITY STORY therefore lands the existing 1-bar ramps on the SUBPHASE jack, with the
  transport jack fed a 2-bar-period source — a re-scaled ramp OR a bar-parity square/LFO (the coarse
  tier needs only the slot, not the sweep). **THE JUMP RULE, RESOLVED (the document's central
  complaint, answered on the merits):** the user is RIGHT that option A rescales the 16-step "no
  jumps between bars" rule — but the rule was a bar==frame ARTIFACT: the code's real rule is the
  within-draw replay bound (PhaseEngine.hpp :130–132 "a jump that would cross the phrase boundary is
  clamped"; Monsoon.cpp :1099/:1103, cap → 32 per the D9 audit). Under the tiers the subphase
  STRUCTURALLY cannot cross the region boundary (the document's claim — TRUE, and it is the merit:
  the old rule restored where it lives, within-region scrubs), while transport-tier jumps cross
  regions BY DESIGN ("jumps within or across sub phase") and clamp at the FRAME — ":1099 at most one
  boundary" re-reads as one boundary OF THE JUMPING TIER'S DOMAIN. **THE HARDWARE (code-verified):**
  the subphase jack is **CV2 in mode E** — CV2's Big-5 modulation routing is already mode-gated to
  the clock/gate origins (`modeSelect < 2`, Monsoon.cpp:1714–1718), so Mode E repurposing it is the
  established takeover verbatim ("mirroring how Mode B repurposes gate1", :843–844); the cv2Mode==5
  quantiser-pitch role yields in mode E (the CV2 role-clash framework, :1715); presence = the tier
  switch (`isConnected()` — the codebase's own gating pattern); unpatched = standalone; plug/unplug
  mid-run re-lands through the existing jump path (a pure-function transition — no accumulator
  surgery). **THE CALIBRATION:** regionSize detents {1, 2, 4, 8, 16} sixteenths — must DIVIDE the
  32-frame (N ∈ {32, 16, 8, 4, 2}); default N = 2 (bar regions — the compatibility configuration); a
  mode-E knob (CV-able later); the PHASE_PARAM knob fallback (:847–859) drives the TRANSPORT tier as
  today. **TIMING UNDER TIERS:** bpm derives from the finest CONNECTED ramp tier (subphase when
  present — a bar-parity SQUARE on the transport jack is a legal transport source with NO meaningful
  velocity, and the bpm validity gate :106 would pass its spikes); sixteenth/quarter edges and the
  mod-4 invariant (:165–166) run on the COMPOSITE walk — D6(a)'s anchors intact; the BAR 1/BAR 2
  LEDs stay stepIndex-driven (D10 untouched — the display reads the LANDING, never the tier
  voltages). **IDEA 3 + THE EXTRACTION (captured, deferred; USER-ENDORSED at D16 — the gates below stand):** "zero new DSP" is optimistic — the
  distribution transform is EMBEDDED in Sands/East (the document's own architecture section says
  so); the prerequisite is the extraction refactor (golden-pinned, its own scope note), and phase
  distortion CHANGES the landed position — engine reads, digest-relevant when scheduled (THE GOLDEN
  LAW). P7 added (post-32): the tier build + its pin list.
  **CONFIRMED (Rodney: "Transport phase will always be mapped to 2 bars 192. It will allow jumps
  across the full 32 steps. With subphase patched , subphase will allow jumps within sibphase and
  Transport phase jumps across the Transport."):** post-ruling ratification, arriving UNPROMPTED
  while the ruling sat unpushed (32ed1f80 after the rebase) — independent convergence on the pinned
  mechanism, the strongest form of ratification; every clause matches, and two of them move from
  adjudication to SPECIFICATION. **(a) THE TRANSPORT SPAN IS ALWAYS 192:** "always" closes the
  Idea-1 door by ratification — the standalone-span override (default 1 bar) is not merely
  declined-by-adjudication, it is excluded-by-specification: the transport tier has NO span
  parameter; its map is D9's (192 positions, 0.3125 V per 16th — the P6 constants) in EVERY
  configuration, subphase patched or not, and the transport jack always wants a 2-bar-period source
  (the compatibility story stands as written). **(b) THE TWO JUMP DOMAINS, PINNED BY THE USER'S OWN
  TWO-SURFACE SENTENCE:** "jumps within sibphase" = the subphase surface is REGION-BOUNDED — the
  structural claim the ruling credited (it cannot cross the region boundary; the restored
  bar-bounded rule lives here); "Transport phase jumps across the Transport" = the transport surface
  crosses regions BY DESIGN and clamps at the FRAME; "jumps across the full 32 steps" is that frame
  — the walk domain 0..31 fully addressable, the within-draw replay bound (PhaseEngine.hpp
  :130–132; Monsoon.cpp:1099, cap 16 → 32 per the D9 audit) the only bound. **(c) AXIS OWNERSHIP IS
  STRUCTURAL, NOT A DISCIPLINE:** the composite position = quantise(transportPhase, regionSize) +
  subphase × regionSize is a pure function of the two instantaneous voltages, so a jump on one tier
  writes only its own term — a transport jump re-slots the coarse term and the fine offset rides
  through untouched (landing = new region, same within-region position: "across the Transport" with
  the subphase preserved); a subphase jump moves only the fine term and the region slot stays. There
  is no state to clobber, so no reader can observe a cross-tier fight — the confirmation's
  two-surface sentence is realized BY the composite formula, not BY a rule about it. The pinned
  detents (regionSize {1, 2, 4, 8, 16} sixteenths, N ∈ {32, 16, 8, 4, 2}) are untouched by the
  confirmation ("jumps within sibphase" works at every detent; the divisor constraint stays a P7
  pin). P7's pin list gains two CONFIRMED entries: transport-span-fixed-at-192 (no span parameter on
  the transport tier) and the two-surface jump domains (region-bounded / frame-clamped).
- **D14 — the 16-stay ring, the third challenge (Rodney: "I think the light ring should stay at 16
  steps and just dim out the steps that are out of scope which changes as we move from first 16 to
  last 16. If only the first 16 or last 16 are out of scope it won't flip bars"):** DECLINED — the
  standing display stands (D8: 32 lamps, FIXED master-step scale; D10: the ghost arc + exclusive BAR
  LEDs; D11: the lap-dim declined). The message is the most precise 16-ring proposal yet — clauses
  1–2 name a real, coherent map — but its three clauses are JOINTLY inconsistent, and the one new
  testable claim (the third) is script-verified FALSE under the only discipline that satisfies the
  second. **THE CREDIT, RECORDED FIRST:** clauses 1–2 together are the CURRENT-LAP GHOST on
  twice-around-16 — each lamp dimmed iff its current-lap master is out of the window, so the dimmed
  set changes as the playhead crosses from the first 16 to the last 16. That is D11's credited
  salvage verbatim ("the D10 ghost map is LAP-CONDITIONAL for free … makes the PASS OBSERVABLE"),
  and it is the flicker D10's reductio named (window 1–20: lamp 5 in-window as 5 on lap 1,
  out-of-window as 21 on lap 2) now embraced as the feature. Nothing in clauses 1–2 is new to the
  log; the new content is the flip claim. **THE FLIP CLAIM, SCRIPT-VERIFIED (the case table):**
  under the current-lap ghost — the only discipline consistent with clause 2 — the claim is
  INVERTED: the one-bar windows are the LOUDEST flips on the ring. Window 1–16 ("only the last 16
  out of scope"): lap 1 ghosts ∅ → lap 2 ghosts ALL 16 — the full ring extinguishes at the tick;
  window 17–32 is the mirror (16 → 0). Spanning windows flip no less (5–20: {1,2,3,4} → {5,…,16},
  16 lamps change state; 1–20: ∅ → {5,…,16}, 12 lamps); the only silent case IN THE TABLE is the
  FULL window (1–32) — no ghosts on either lap (on 16 lamps silence requires BAR-SYMMETRIC scope —
  W closed under the lamp pairing k ↔ k+16 — degenerate exceptions, not the rule) — and that case
  is D11 count (6)'s DEFECT case (the tell vanishes at the
  feature's maximum), not a virtue: "it won't flip bars" is true of exactly the window where the
  display law already failed. **THE DISCIPLINE THAT WOULD MAKE THE CLAIM TRUE IS WORSE, VERIFIED:**
  the FUSED ghost (dim iff BOTH masters k and k+16 are out of the window — the reading in which
  one-bar windows never flip) is blind and lying precisely where the 32-step feature lives: every
  window containing one full bar renders ZERO ghosts (1–20 lights all 16 lamps on both laps while
  masters 21–32 are out of scope — indistinguishable from 1–32; 5–20 likewise); and for bar-1
  windows it states the BAR-1 exclusions on BOTH laps (3–10: {1,2,11–16} on both laps), so during
  bar 2 the lit lamps PRESENT OUT-OF-SCOPE MASTERS AS IN-SCOPE (lamp 3 lit while master 19 plays
  out of scope) — the map shows one bar's scope and misstates the other's; identity-by-fusion (D11
  count 7) with D8's alias returned as a lit-lamp ambiguity. **THE THREE CLAUSES CANNOT ALL HOLD:**
  clause 2 (the dimmed set changes at the crossing) and clause 3 (no flip at one-bar windows) are
  mutually exclusive — the discipline that makes the set change flips MAXIMALLY at one-bar windows
  (16 of 16 lamps); the discipline that forbids the flip kills the change and the scope display with
  it (zero ghosts for every full-bar-containing window). **WHAT THE STANDING MAP ALREADY DELIVERS —
  and where the message's true want lives:** the out-of-scope dimming IS the D10 ghost arc verbatim;
  and the no-flip want is ALSO granted by the standing map and by nothing else on the table — on 32
  lamps each lamp has ONE master, so the arc is lap-INDEPENDENT (script-verified: identical every
  lap at every window) — the scope display never flickers at the crossing; the bar dynamic lives in
  the BAR 1/BAR 2 LEDs (swap at the tick) and the playhead's half-jump (lamp 16 → lamp 17 IS the
  boundary, D8 count 1); and the 2-bar comparison stays visible (both bars' arcs coexist on the
  constant scale — D11 count 8's unseeable-on-16 case). D8's five counts and D11's three stand
  unanswered (injectivity; the new % 16 the twice-around path would force where P4 sweeps them out;
  pass-blindness; the marked 2-bar reading; the EXTENSION-LAW special case; tell-inverse-to-window;
  fusion; disjoint arcs). The D11 P6 pin stands: at the full window the BAR LEDs alone carry the lap
  tell — the one case clause 3 calls silent, and the one case the 16-lap scheme cannot render.
- **D15 — the 16-stay ring restated, the in-ring bar indicator, the fourth challenge (Rodney: "I
  dont think you get it at all. We have the light ring as it is now. We add a simple indicator bar
  1/bar 2 perhaps inside the ring, space unused. Playing stepis 1 to 16, bar 1 indicator lights.
  Any ring steps in 1..16 outside range are dimmed. Playing steps 17 to 32 bar 1 indicator dims
  bar 2 indicator lights. Any ring steps in 17-32 outside range get dimmed. The whole range might
  cross bars or just be inside 1 bar, first or 2nd. This approach is scalable if we moved to 64
  steps."):** ACCEPTED — the 16-lamp twice-around ring with the CURRENT-LAP ghost and D10's BAR
  1/BAR 2 LEDs REPOSITIONED inside the ring's unused center REPLACES the D8 standing 32-lamp map
  as the display law. **THE D14 MODEL ERROR, CORRECTED FIRST — the decline's foundation falls:**
  D14's flip table evaluated ghost(lap 1) → ghost(lap 2) at a FROZEN window — a lap transition
  that cannot occur. The display lap follows the PLAYING step (the message's own sentences; D10's
  split test `stepIndex >= 16`), and the playhead is CONFINED TO THE WINDOW: out-of-window
  landings park at the window start (SequencerEngine.cpp:788–792) and advancePlayhead wraps
  WITHIN the window (:486–488 — out-of-window advance snaps back to startStep; the walk IS the
  playhead's sweep, D11's own reading of :486/:491). A window confined to one bar never plays the
  other half: no crossing, no tick, no flip — Bar 1 lit, ghosts per lap 1, PERMANENTLY STABLE.
  The corrected table (scripts/d15_ring_flip_table.py, the playhead-confined model) verifies: the
  one-bar windows are the SILENT class — the claim as stated is TRUE, by CONFINEMENT, not by the
  fused discipline (every confined window is silent, not only the exactly-16 pair: 3–10 and 20–27
  in the table); the flip is real EXACTLY for tick-straddling windows, where BOTH laps render and
  the playhead crosses ((5,20): {1..4} → {5..16}, 16 lamps; (9,24): {1..8} → {9..16}, 16; (1,20):
  ∅ → {5..16}, 12; (16,17): {1..15} → {2..16}, 2 — D14's straddling rows stand verbatim); the
  full window renders no ghosts on either lap and the BAR LEDs alone carry the lap (the P6 pin,
  shared by both schemes). D14's "script-verified INVERTED" finding and its fused-discipline
  reductio are RETRACTED as moot — the fused discipline was the only reading that made clause 3
  hold WHILE ASSUMING the lap switch fires at a frozen one-bar window; under the current-lap
  discipline the switch never fires there. The message's crossing sentence ("The whole range
  might cross bars or just be inside 1 bar") is the designed swap made explicit: indicator, dim
  set and lamp mapping move TOGETHER at the playhead's tick crossing. **THE FOURTH CHALLENGE'S
  NEW CONTENT (why the decline does not stand):** the indicator is promoted from D10's decoration
  to a FIRST-CLASS address component — the pair (lamp, indicator) is INJECTIVE over 1..32, the
  answer to D8 count 1 the D14 record said stood unanswered — and the 64-step scalability
  sentence reframes D8 count 5: at 64 steps the 32-lamp FIXED map hits the SAME alias (lamp s =
  step mod 32) and needs an indicator anyway, while (lamp, indicator) extends verbatim (Bar 1–4,
  the ring stays 16) — the 16-stay is not the special case; it is the scalable pattern. **THE
  COUNTS, RE-ADJUDICATED:** D8 count 1 (injectivity) — the alias is DISAMBIGUATED, not removed;
  the cost is the two-surface read, accepted by the message's design (Rodney retracts his own D8
  rationale "Master steps are directly indicated in it"). Count 2 (the new % 16) — ACCEPTED as
  the ONE display-path exception (`lightIdx == stepIndex % 16` in the lamp feed only; no engine
  read, no digest — THE GOLDEN LAW holds; P4 must PERMIT it, pinned). Count 3 (pass-blindness) —
  REPAIRED by the indicator, as D11's credit conceded and the message's explicit display
  supplies. Count 4 (the marked 2-bar reading) — stands as an ACCEPTED LOSS: both bars' quarters
  collapse onto the same four lamps. Count 5 — reframed above. D11 count 6 (tell inverse to
  window size) — DISSOLVED: the tell no longer rides the ghost; the indicator carries it at EVERY
  window including the full one. Count 7 (identity by fusion) — the fusion accepted; the ghost
  level returns to DECORATION (the lap read is two discrete LEDs, not a brightness step). Count 8
  (two disjoint arcs) — stands as an accepted cost, now PARSEABLE BY DESIGN ("parseable only WITH
  the lap bit" — the message supplies exactly that bit). **WHAT THE STANDING MAP YIELDS (the
  honest ledger):** the lap-INDEPENDENT arc (no re-scope flicker at the crossing) and the
  simultaneous 2-bar comparison are LOST; the current-lap map re-scopes at every tick crossing —
  up to 16 lamps, ATOMIC with the indicator swap and the playhead's lamp 16 → lamp 1 jump: three
  simultaneous events, the boundary made LOUD (the design's intent: "bar 1 indicator dims bar 2
  indicator lights"). The comet trail renders lamp-local and is unaffected. **THE PINS (the
  display law):** the ring stays 16 lamps, twice-around (lamp k carries masters k and k+16); the
  lap is the PLAYHEAD'S half (`stepIndex >= 16`, D10's split test — never a pass counter; D11's
  alternation stays DECLINED); the ghost is CURRENT-LAP (lamp k ghosted iff its current-lap
  master is out of the window), values per the D10 hierarchy verbatim (playhead 1.0 > trail 0.5 >
  in-window 0.35 > rest 0.07 > ghost); the BAR 1/BAR 2 LEDs are D10 verbatim (lit EXCLUSIVELY,
  the stepIndex split, both silent in mode D) REPOSITIONED inside the ring's unused center (P5
  calibration, pinned by the message); jumps (D13's two surfaces) re-scope the display
  ATOMICALLY at the LANDING — indicator, dim set and mapping move together; the display reads
  the LANDING, never the tier voltages (the D13 pin); CV-swept windows (D3): a confined window
  swept across the tick BECOMES straddling and the flip fires thereafter — the flip tracks the
  LIVE window, the message's "changes as we move" generalized. **THE 64-STEP NOTE (pinned as
  rationale, not commitment):** the pattern extends as ring-16 + k-way indicator (Bar 1–4 at 64);
  the pair stays injective; no lamp growth. **THE PLAN SECTIONS AMENDED:** F/Ring and F/Ring
  feed (the 16-stay form; the sanctioned % 16), P5 (the 16+2 anchor set), P6 (the D15 ring-map
  pins). D8's 32-lamp FIXED map is demoted to the recorded alternative; D10's ghost arc is
  re-scoped to the current-lap form; D14's decline is SUPERSEDED (its one-bar rows fabricated,
  its straddling rows confirmed).

- **D16 — the convergence ratified, the arc closed (Rodney: "You argued hard against 16 lamp ring and
  single bar phase and now we have a much better solution that includes both as well as 32 step
  transport phase. Phase going through distribution engine is also a great idea."):** ACCEPTED AS THE
  STANDING CHARACTERIZATION of the arc D12 → D14 → D13 → D15, and as a STATUS CHANGE for Idea 3. No
  new mechanism, NO PIN CHANGES — the message ratifies the converged architecture and weights the
  backlog; every P7 pin, gate and sequencing decision stands where D13/D15 left it. **THE ARC, READ
  INTO THE LEDGER — the declines were load-bearing:** each hard decline located the defect in a
  MECHANISM while the WANT walked through. D12 declined the 1-bar SPAN (the parity bit cannot live in
  the bar-local accumulator — zero bits of bar identity on the wire, wrong-half-on-reconnect,
  silent); the want — "phase = where in the bar" — now lives as the SUBPHASE tier at the
  compatibility configuration (regionSize = 1 bar, N = 2): the existing 1-bar ramps land on the
  subphase jack BIT-IDENTICALLY (the D13 compatibility story), bar identity carried by the transport
  tier's 192 sweep — a VOLTAGE, D12's own demand, met in the new channel. D14 declined the 16-stay
  CLAUSE SET as literally stated (its three clauses jointly inconsistent under the frozen-window
  model); the want — 16 lamps, out-of-scope dimming, no flip inside a bar — now lives as the D15
  display law (twice-around-16, the CURRENT-LAP ghost, the in-ring BAR 1/BAR 2 indicator; the one-bar
  silence holds BY CONFINEMENT — the corrected model, not the fused discipline D14 refuted).
  "Includes both" is therefore exact for the WANTS, with the mechanisms replaced by the forms that
  survive the script table and the reseed math; the third limb, the 32-step transport phase, is D9's
  192 map as the transport tier — "always" per the D13 CONFIRMED ratification. **IDEA 3 ELEVATED TO
  USER-ENDORSED:** "Phase going through distribution engine is also a great idea" is the message's
  new content — the D13 status moves from CAPTURED-DEFERRED to USER-ENDORSED, STILL DEFERRED; the
  deferral logic is restated so the endorsement cannot be misread as a build order: the transform is
  EMBEDDED in Sands/East and its extraction refactor is the prerequisite (golden-pinned, its own
  scope note); phase distortion CHANGES the landed position, so engine reads and the digest are in
  scope when scheduled (THE GOLDEN LAW); sequencing stays after 32 lands, post-P7 — the endorsement
  weights the backlog, it does not move the gate. **THE PROCESS NOTE, LOGGED:** the convergence the
  message describes is the adjudication discipline working as designed — decline the inconsistent
  mechanism, credit the want, and the accepted form contains both proposals' wants without their
  defects; the D14 correction was itself load-bearing (exposing the frozen-window error is what made
  the D15 acceptance coherent). **PLAN SECTIONS TOUCHED:** the D13 block (the Idea-3 status marker,
  inline), the P7 entry (the endorsement note); Sequencing untouched.

- **D17 — the UI-memcopy question adjudicated: the fingerprint plane IS the gate, the display path is
  already zero-copy, and the copied data is the mutable set BY DESIGN (Rodney: "please log it and then
  push this and the audit document…"):** ADJUDICATED AS ASKED — the question's premise answered
  INVERTED, and the one genuine reduction CAPTURED-DEFERRED. Asked against Finding D of the
  array/container audit (the ~20 `sizeof`-memcpy sites of the snapshot plane), in two limbs: could the
  UI memcopy be avoided, and is the copied data unmodified/constant/read-only. **THE FINDING, THREE
  FAMILIES:** (1) the `memcpy(dst, &img, sizeof img)` sites (MonsoonSandsManager.cpp:1065,
  MonsoonExpanderManager.cpp:817, StraitsEastSandsVisual.hpp:322, StraitsSandsMacroVisual.cpp:1291)
  are RateManager PRODUCER FINGERPRINTS, not UI handoffs — the contract (RateManager.hpp:95-100)
  serializes every input the produce reads so dispatch() can memcmp the image against the shadow of
  the last run and SKIP the produce bit-identically (the SweepSig skip-on-unchanged pattern
  generalized, RateManager.hpp:19-25); audio-thread, single-threaded (:38), tier cadence (Control
  ≈1.5 kHz; the East gate-mod scan at the /8 detector cadence) — the UI never sees that buffer.
  (2) The shadow captures (v1SpreadCapture_, sweepSigCapture, captureRemapSig_) are the same gates'
  other half — capture-on-change behind memcmp-first dirty checks, already minimized. (3) The ACTUAL
  display handoff has NO memcpy today: the produce writes the visual's members in place
  (`macroVis->macroBase[lane][0] = baseLen`, MonsoonSandsManager.cpp:1002-1007) and draw() reads the
  produced rows by reference (`StepProbRow& row = stepProb_[l]`, StraitsSandsMacroVisual.cpp:1210) —
  Rack's own params/lights convention, tearing benign at display rate; an immutable-snapshot mailbox
  would ADD a per-tick audio-side copy to fix a race that does not matter. **THE READ-ONLY LIMB,
  ANSWERED INVERTED:** the fingerprint's job is to DETECT modification, so it must contain exactly
  the mutable set — knob stores, CV jacks, engine-published cells, the produced cells read back for
  self-healing, the module pointer identities (mutable on patch/unpatch); the constant data (lane
  names, warp tables, config) is in NO image; anything mutable left OUT is the stale-state bug the
  contract names ("THE FINGERPRINT MUST COVER EVERY INPUT THE PRODUCE READS; a missed input = stale
  state", RateManager.hpp:24-25). The writer-agnostic byte-image IS the healing property — ANY
  external writer breaks the compare and the next tick re-runs (SweepSigGate.hpp:24-26;
  MonsoonSandsManager.cpp:1151-1154) — which dirty flags/epoch counters forfeit at the first
  forgotten bump; the codebase's own scar is 70dd837 (the step-NUMBER-keyed cache, Macro-only CPU
  0.4% → ~3%, restored by CC_PROMPT_restore_sweepsig_gate_START). **THE VERDICT:** no memcpy in the
  plane is removed. The one real reduction is CAPTURED-DEFERRED — the compare-in-place dispatch
  variant (a CompareFn beside SnapshotFn: memcmp live state against the stored shadow per field,
  capture into it only on change — sweepSigChanged already proves the shape, SweepSigGate.hpp:101-118),
  eliminating the scratch copy (RateManager.hpp:122-124) on the unchanged path, where it is pure
  overhead; deferred as measured noise — worst image ≈2.5 KB (RateManager.hpp:78-80), the East scan
  ≈1 KB, at 1.5-6 kHz L1-resident copies of ~100-200 ns gating a ~22.7 µs resolve (SweepSigGate.hpp:7)
  — the gate pays ~100-200× on the first skipped run. **THE AUDIT TIE:** this is Finding D restated
  as mechanics — the images must mirror the source arrays' byte layout; `std::array` keeps every
  `sizeof`-memcpy compiling identically, vector silently degrades each to a 24-byte handle copy;
  P-A…P-E and the Sequencing gate are unmoved. The quote truncates before the message's one-time PAT
  clause — credentials never enter the ledger. **PLAN SECTIONS TOUCHED:** this block only; the
  array/container audit rides the same push as its own commit (3e26aa52); Sequencing untouched.

- **D18 — the D17 deferral lifted by user order: the compare-in-place variant IMPLEMENTED as code on a
  branch off the lane tip, then ADOPTED on Rodney's measurement (Rodney: "can you do D17 on a branch off
  lane_expander_refactor, it seems worthwhile for CPU saving"):** the CAPTURED-DEFERRED status of D17's
  one genuine reduction is LIFTED — the compare-in-place dispatch is now code. Branch
  `feat/d17-compare-in-place` off the lane-expander-refactor tip (81fc7826), one commit (d0aa7512),
  pushed. **THE SHAPE:** a `CompareFn`+`CaptureFn` pair beside the legacy `SnapshotFn` (the legacy path
  and its no-input run-once semantics untouched): compare() walks the LIVE inputs field-by-field against
  the declaring site's OWN typed shadow — per-field memcmp, EARLY-OUT on the first difference, no image
  build, no scratch copy through the manager; capture() stores the POST-RUN image after a produce (the
  one-run-per-change law, byte-exact per field). The shadow lives at the declaring site — a typed,
  per-instance member, never in the manager: PublishSig (MonsoonSandsManager), PopulationSig
  (MonsoonExpanderManager, the ≈2.5 KB packed image D17-typed), DirDelegSig (StraitsEastSandsVisual —
  the hottest fingerprint in the plane, Sample tier /8 ≈ 6 kHz, where the dropped Img build + scratch
  copy is the largest single saving). The pointer words compare FIRST so a presence flip short-circuits
  the walk; the compare is gated on everRan (never consulted before the first capture — the shadow
  would not exist yet). **THE MIRROR LAW:** the pair must cover the SAME field set in the SAME order —
  the compare is the capture's mirror, one enumeration each, adjacent functions (drift = stale state);
  the old publishGlobalFingerprint_ / macroPopulationFingerprint_ are DELETED (deletion is the
  enforcement); the 1-byte sweep sig (StraitsSandsMacroVisual) stays legacy BY DESIGN — the image IS
  one byte, nothing to save. **THE CONTRACT HOLDS:** every-input coverage is structural in the pair, and
  R9 pins the property on the real manager shape (never-compare-before-capture; one-ULP byte-exactness;
  per-field coverage — every captured field's perturbation re-runs; foreign writes re-assert — the
  70dd837 scar class guarded); R10 pins legacy/compare coexistence + tier discipline; the full battery
  is green (93/93 suites + the anchor/bind audit 9/9), and the three changed plugin TUs show ZERO new
  errors vs base on a differential syntax pass (the real-SDK smoke remains CI's gate). **THE
  MEASUREMENT, RODNEY'S VERDICT:** "it seems to save maybe .1% or .2% of cpu so not earth shattering but
  not nothing so worth it." — the deferral's "measured noise" estimate confirmed empirically and
  overridden as worth keeping: ≈0.1–0.2% CPU for the removal of a pure-overhead copy on the unchanged
  path, with the skip semantics bit-identical. The variant stands ADOPTED on the branch, pending that
  branch's merge. **PLAN SECTIONS TOUCHED:** this block only; Sequencing untouched.

- **D19 — the array audit EXECUTED: P-A…P-E landed as four staged commits on a branch off
  feat/position-holders (the base corrected from the D17 line), the battery green at every gate, the
  guardrail installed (Rodney: "on feat/d17-compare-in-place let's do P-A to P-E from the audit doc",
  "you can do it on top of feat/position-holders instead of lane_expander_refactor", "Please push to
  github and record ledger entry"):** the array/container audit's staged modernization plan
  (docs/design/ARRAY_CONTAINER_AUDIT.md, logged 3e26aa52) is now CODE — the PhiloxRng.hpp exemplar rule
  throughout: std::array, named constants, zero allocation, ZERO behavior change, the battery the gate
  at every stage. **THE BASE, CORRECTED:** the assignment named the D17 line; Rodney's correction moved
  it — branch `refactor/array-containers` off feat/position-holders (e9ad5fe0, the merge carrying the
  position-holder model), NOT the lane-expander/D17 line; four commits, pushed as a NEW remote branch
  (tip 6e4dabb6), and this entry logged on Rodney's instruction. **P-A — CONSTANTS FIRST (dffb70c7; no
  type changes, no behavior):** the magic capacities get their ONE spelling per file, the spelling each
  file already speaks — ChangeAlleyTransforms.hpp's ~40 bare 16s → ca::N_VOICES (kCorrPage=16 is a
  DIFFERENT 16, the RNG page, and stays), PatternEngine's class-local kVoiceSlots + the PL_LANES enum
  hoisted above the members it sizes, SequencerEngine's kVoiceSlots hoisted + macroLOR_ NUM_STRANDS-sized
  + the eight [kVoiceSlots][NUM_STRANDS] lane planes, RhythmDecisionCore's kVoiceSlots for the
  containment-graph planes, the Straits visuals aliased from VoiceResolver::kVoiceSlots, Macro's
  StepProbRow::val + the [POLY_LANES][4] planes; the STEP-16 (the row length) stays literal until
  STEPS_32 names it; VoiceBank::slots[16] stays literal (standalone struct, the constant unreachable);
  the static_assert doors hold (VoiceResolver.hpp:178, PL_LANES==kLaneIdCount, POLY_LANES==
  POLY_LANE_COUNT); test_ca_qmix_source_select's replica now binds the PRODUCTION ca::N_VOICES (its
  local 16 deleted). **P-B — std::array FOR MEMBER STATE (8bf03aed; type-only, byte-identical):**
  T name[N] → std::array<T,N> file by file — PatternEngine (the random_/warped_ planes nested, the 28
  [kVoiceSlots][16] planes via the Matrix2 alias std::array<std::array<T,Cols>,Rows>, the row readers
  re-typed std::array<float,16>&, indexing unchanged), SequencerEngine (VoiceBank::slots, the 3-D LOR
  stores, the lane-walk planes, the per-voice scalars, the East mask planes), RhythmDecisionCore
  (graphWords stays a POINTER — it aliases containmentParents, re-wired .data()), MonsoonChangeAlleyV2
  (TransformUndoSnapshot AND its TransformUndoAction rack-history mirror IN LOCKSTEP — byte-coupled by
  field-wise copy), and the MIRROR STRUCTS IN THE SAME SERIES per the audit's lockstep rule
  (SweepSigGate's SweepSig, MonsoonSandsManager's RemapSig/TopoSig/V1SpreadSig/MirrorSig — .data() where
  the memcmp/memcpy pairs cross; the WarpBank side stays raw C, not in the audit's named scope), the
  Straits visuals' fingerprint planes the managers sizeof; char string buffers (rangeLabel_[16]) and the
  label tables stay raw per the allowlist classes; sizeof(std::array<T,N>) == sizeof(T[N]) — every
  Finding-D site compiles byte-identical, Finding D survives UNTOUCHED. **P-C — SCRATCH DISCIPLINE
  (8a2a8802):** the Finding-B bug class deleted at the root — every function-local scratch crossing a
  call boundary becomes std::array ZERO-INIT (motionV x4 with .data() at the 8 pointer crossings, the CA
  transform scratch, varyNoteIndex's rungs/weights, the remap mono snapshot, the CA V2 true-reverse +
  pending-order arrays, the Macro sweep stepArr x2); the TLS buffers (Finding C) become INSTANCE members
  — the remap poly snapshot → remapSnapR..L (Matrix2 members, ~7 KB/instance vs ~90 KB resident, no
  objection) and the Philox draw buffers rhythmDrawBuf/melodyDrawBuf/qmixDrawBuf (mutable, ALIGNAS(16)
  spelled EXPLICITLY — the TLS arrays got 16-byte alignment from the ABI and fillBlock4xSimd's
  _mm_store_ps demands it; ASan caught the first build's misaligned store as a SEGV, the battery proves
  the fix); monsoonProfGateWhy DELETED — no reader anywhere in src/, dead mutable file-static is a
  finding, not furniture (sweepSigWhy stays); two structural pins re-worded to the new call shape
  (stepArr → stepArr.data(), the same gate call). **P-D — RECORDED, ZERO CHANGES:** the audit's vector
  set verified IN PLACE and stays vector — connectedHostIds_ (the expander ring), pendingSpreadArcs x2 +
  leftAttenuverters (the UI widget collections); zero engine-state vector candidates, the honest zero.
  **P-E — THE GUARDRAIL (6e4dabb6):** test/lint_array_guard.py registered in run_all.sh next to the
  anchor/bind audit — the battery now FAILS on any NEW bare C-array declaration in src/dsp/** and
  src/ui/** outside test/lint_array_guard_allowlist.txt: statement-level matcher (parameters,
  expressions, subscripts never match), the exempt classes never match (char string buffers, label
  tables, NVGcolor), the allowlist keyed path:name on the DECLARED NAME so the baseline survives line
  drift — the 137-site pre-gate residue outside the audit's named scope, 121 unique keys, each future
  removal shrinks it, each NEW site needs an allowlist line WITH A REASON in the commit message or a
  std::array conversion; fatal by default, ARRAY_GUARD_STRICT=0 the local hatch (the
  MODULE_LOOKUP_STRICT precedent); proven the repo way — a planted float plantedProof[16] in
  SpreadInterp.hpp went RED (rc=1, the site named), removed, green again. **VERIFICATION at every
  gate:** full battery green (94/94 suites + anchor/bind audit 9/9; + array-guard 0 violations at P-E);
  the CXX20 warning sweep 0 diagnostics; consumer-TU differentials 0 new errors vs base at P-B/P-C (the
  plugin-TU smoke remains CI's gate); Rack feel-test not triggered — type-identical storage, nothing
  visual moved. **THE AUDIT'S FINDINGS CLOSED:** A by P-A, B by P-C's zero-init, C by P-C's instance
  members, D survives untouched by the sizeof guarantee + the same-commit lockstep rule. **PLAN SECTIONS
  TOUCHED:** this block only; Sequencing untouched.

## Sequencing

Own branch off the lane tip AFTER feat/position-holders lands (never ride the same train — this touches
the copula/CA/reversibility/hot-path frame the position-holder work just stabilized). P0 lands first as
the reviewable no-op; the resize commit is P1–P4; surfaces and pins follow. The battery is the safety
net at every gate, same discipline as the position-holder refactor.

════════════════════════════════════════════════════════════════════════════════
## SETTLED DECISIONS (Rodney — supersedes earlier back-and-forth in this doc)
1. LIGHT RING: stays ONE BAR (16 lamps) with a BAR INDICATOR. 32 steps = two sweeps, bar indicator
   disambiguates. This is the SCALABLE approach — extends to 64 steps without re-lamping the ring.
   (The earlier "32 fixed lamps" option is rejected.)
2. SCALING TARGET: 64 steps is the eventual ceiling. Real 64-step blockers are only: (a) the Sands probability
   bar horizontal shrinkage (display), (b) possibly compute. Design for 64 extensibility; don't build it yet.
3. PHASE, INITIAL 32-STEP WORK: phase is LEFT AS IS (the existing one-bar / 96-per-bar mapping). Do NOT
   change phase in the 32-step build.
4. PHASE, LATER: expand transport phase to 192 (2 bars). That becomes the transport-phase base, which the
   subphase / distortion work then builds on. Sequenced AFTER the 32-step build, not part of it.
5. PHASE DIRECTION (future, not the 32 build): transport phase (192, not stochastic in itself) + subphase
   (quotient/remainder, quantised transport selects region) + distortion through the MO distribution
   primitive (warp/tilt/atoms). Naming: this is the MO distribution layer applied to a deterministic
   transport; NOT an "MO copula" (transport is not usually stochastic). Clock mode is the base; gate mode has
   subgates + advance probability, phase will have subphase + distortion.

## BUILD PLAN — 32 steps (the next major piece)
PREREQUISITES (now satisfied on trunk after the merges): position-holders (no global counter; length-
agnostic holders), UAS (per-voice addressing), rhythmPattern deleted, polarity audited, DNA_LCM dead.

STAGE 1 — STRUCTURAL (bit-identical at 16; the extension-of-16 proof)
- Pattern/step length accepts 1..32; default and all existing patches stay 16. Golden battery must be
  bit-identical for every ≤16 configuration. This is the regression gate for the whole feature.
- Holders already wrap at their own length, so the walk/addressing side should need little change; audit
  the few remaining fixed-16 literals (& 0x0F, % 16, < 16, the 16-sized arrays and clamps) and make them
  pattern-length-aware or 32-capable.
STAGE 2 — DATA STORAGE
- Per-step arrays (step values, mask/painted bits, Sands probability storage) extended to 32 entries.
  Mask is 16-bit today (uint16 maskPainted_) — widen to 32-bit or a bit-pair; this is a real change, scope it
  carefully. Keep ≤16 bit-identical.
STAGE 3 — DISPLAY
- Sands probability grid: 32 columns; the horizontal-shrinkage concern is real and is the first 64-step
  blocker, so design the grid to be column-count-parameterised rather than hardcoded 16.
- Light ring: 16 lamps, bar indicator (bar 1 / bar 2), single sweep = one bar. Settled, not reopened.
- Step-count control: 16 / 32 selector (a pattern-length parameter). Decide menu vs knob when building.
STAGE 4 — CONTROL + PERSISTENCE
- Pattern-length parameter, persisted; ≤16 patches load unchanged (default 16).
STAGE 5 — VERIFY
- Goldens ≤16 bit-identical (the gate). New 32-step pins: a 32-step walk wraps at 32, both bars reachable,
  the bar indicator tracks the correct bar, mask/painted bits address all 32 steps.

## NOT IN THIS BUILD
Phase change (stays as is), 64 steps, transport phase 192, subphase, phase distortion, the distribution
primitive extraction. All sequenced after; recorded in PHASE_TIER_SYSTEM_IDEA.md.

════════════════════════════════════════════════════════════════════════════════
## DECIDED — Stage 2 mask representation (Rodney)
- Per-voice East pattern and the macro pattern become uint64_t (std::array<uint64_t, kVoiceSlots> for East).
  uint64 covers the 64-step ceiling, so there is no second widening later.
- Length-masked: every read and write masks with lenMask = (1ULL << L) - 1. Replaces the 65535 clamp,
  the `c < 16` checks and the 1<<c magic.
- Default all-on for the current length (behaviour-identical at ≤16).
- Persistence: NO migration. Patch compatibility is not a concern for this unreleased code. Save the new
  integer representation under a fresh key; old float-keyed saves are simply not read.
- Stage 1 gate still applies: ≤16 BEHAVIOUR must be bit-identical (outputs and goldens), even though old
  files are not loaded.

## STAGES 2–5 — OUTLINE ONLY. Prompts deferred until Stage 1's classification table is back.
Reason: Stage 1's (A)/(B)/(C) table sets the real size of Stage 2. Writing Stage 2 prompts now would be a guess,
and guessed specs are where invent-vs-reuse errors come from. Outline + gates + open decisions are recorded
here; CC-executable prompts follow once Stage 1 reports.

### Stage 2 — storage (LARGEST; not a small stage)
Grounded sizing (trunk): ~40 per-step arrays sized [16] across PatternEngine (slewed*, cached*, melody/pitch
caches, the [kVoiceSlots][16] strand buffers) and Monsoon (per-voice editor arrays, the voice-LOR and mask
editor arrays), plus four uint16 mask fields in SequencerEngine (maskPainted_, maskAnchored_,
maskLastPushed_, maskMacroPainted_).
Key hazards:
- maskEastPatternBits / maskEastPatternBits are stored as float[16] (Monsoon.hpp ~969). A 32-bit mask does NOT
  round-trip exactly through a float (24-bit mantissa). Widening these needs integer storage or a bit-pair
  split. Decide before writing the Stage 2 prompt.
- Persistence keys for every widened array must keep ≤16 patches loading identically (default 16).
OPEN DECISIONS (Rodney, before Stage 2 prompt):
  (a) mask representation: DECIDED — uint64, length-masked (see DECIDED block below)
  (b) which per-step arrays truly need 32 entries vs can stay 16 (e.g. melody caches may be length-gated).
Gate: ≤16 goldens bit-identical; 32-entry arrays zero-filled beyond length; persistence round-trip ≤16 exact.

### Stage 3 — display
Sands probability grid: 32 columns, parameterised by column count (the first 64-step blocker: horizontal
shrinkage — design so 64 is a constant change). Ring: 16 lamps + bar indicator (settled). Length control 16/32.
OPEN: menu vs knob for the 16/32 control.
Gate: ≤16 display pixel-identical where testable; 32 layout reviewed in Rack (human review — no automated gate).

### Stage 4 — control + persistence
Pattern-length parameter, persisted; absent key → 16. Gate: old patches load at 16 unchanged; round-trip 32.

### Stage 5 — verify
Goldens ≤16 bit-identical (the standing gate). New 32-step pins: wrap at 32; both bars reachable; bar indicator
tracks bar; mask bits address all 32 steps; subgate/advance/per-voice interaction at length 32 (the combined case,
per the earlier lesson that clean merges can still have interaction bugs).

### Not part of 32 (deferred, sequenced after)
64 steps; transport phase 192; subphase; phase distortion; distribution-primitive extraction.

════════════════════════════════════════════════════════════════════════════════
## STAGE 1 REVIEW (PR #310) AND REVISED STAGES 2–5
### Stage 1 as delivered (feat/steps-32-stage1)
Delivered: kMaxPatternSteps=32 (ceiling), kPatternLengthCap=16 (usable bound, the one switch), the length CV
mapping derived from the cap, the lane-length clamp derived from the cap, the 113-check pins test.
Open items to close before merge:
1. The length param range derives from kMaxPatternSteps, so the knob offers 32 and silently clamps to 16.
   FIX: derive the param range from kPatternLengthCap. The knob then never offers an undeliverable length, and
   Stage 2 changes one constant that moves the range with it.
2. The ≤16 golden gate has not run. The commit claims bit-identical "by construction" and defers the goldens to
   a follow-up battery. The full battery must pass on the PR before merge, not after.
3. The ~381-site classification table lives only in the PR description. Commit it to docs/design (e.g.
   STEPS_32_CLASSIFICATION.md) so the audit record survives the PR.

### Stage 2 — storage (REVISED: mostly constant changes, because the array refactor is on trunk)
Trunk now has std::array member state (PR 305). The 16-wide row dimension was deliberately left literal for
STEPS_32 to name, so Stage 2 names it once and widens it.
- Introduce kStepStorage (= 32 at Stage 2; the storage bound), separate from kPatternLengthCap (the usable cap).
  Stage 2 flips the cap to 32; storage is already 32 wide, so no per-site widening.
- Replace the 16-literal row dimension (54 sites in PatternEngine.hpp alone) with kStepStorage. Mechanical,
  but the per-site classification still applies: row-length uses change, unrelated 16s don't.
- Masks: uint64_t, length-masked, DECIDED (see DECIDED block). Covers 64 later with no re-widening.
- The offset param: 0..15 becomes 0..(cap-1), derived not literal. Class B today, resolved here.
- The CV rescale *(cap-1) lands automatically (the D1 *31), because it is already cap-derived.
- Gate: ≤16 goldens bit-identical with cap still 16 (pre-flip), then the flip is a separate commit with its own
  gate: the 32-entry arrays zero-filled beyond length, and persistence round-trips at 16 exactly.
- Stage 2 is split into two commits: (2a) name kStepStorage and widen storage with cap still 16 (pure refactor,
  goldens bit-identical); (2b) flip the cap to 32 (behaviour change, its own pins).

### Stage 3 — display (unchanged in substance)
Sands 32-column grid parameterised by column count. Ring: 16 lamps + bar indicator (settled). Length control
follows the cap. Gate: ≤16 pixel-identical where testable; 32 layout reviewed in Rack.

### Stage 4 — control and persistence (unchanged)
Pattern-length param persisted; absent key → 16. Old patches load unchanged. Patch compatibility is not a
concern for this unreleased code (decided); the gate is behaviour at ≤16, not file compatibility.

### Stage 5 — verify (unchanged)
≤16 goldens bit-identical; new 32-step pins: wrap at 32, both bars reachable, bar indicator tracks bar, mask
bits address all 32 steps; combined subgate/advance/per-voice interaction at length 32.

### Not part of 32 (sequenced after)
64 steps (ceiling change + display; masks already uint64), transport phase 192, subphase, phase distortion,
distribution-primitive extraction.

════════════════════════════════════════════════════════════════════════════════
## BAR INDICATOR — inner disc (Rodney's clock analogy) — DISPLAY SPEC
Analogy: the light ring is the second hand (step within a bar); the inner disc is the minute hand (which bar).

GEOMETRY
- The disc sits inside the 16-lamp ring, in the currently unused centre.
- 16-step pattern: one disc (no division). Either hidden or lit uniformly; never show a half-lit disc, since that
  implies a second bar that does not exist.
- 32-step pattern: cut along the horizontal centre into two hemispheres. Upper = bar 1, lower = bar 2.
- 64-step pattern (future): quarters, one per bar.

STATE
- The active bar's segment is lit. Every other segment dims.
- Dim level must be clearly distinct from both the lit state and the unlit ring lamps. Verify at real panel
  brightness in Rack, not on a screenshot.

INVARIANTS
1. The disc reports the ABSOLUTE pattern bar (engine position), not the voice LOR window. Display reads engine
   position, never recomputes it. Stating this here so the disc is never wired to the LOR later.
2. Flip timing: the active segment changes on the same tick as lamp 0 of the new bar. A lag of even one step
   breaks the clock reading and makes the bar boundary ambiguous.
3. The bar is always 16 steps. Pattern length changes only how many divisions the disc has, never what a bar means.

GATE (Stage 3)
- ≤16: visual output unchanged apart from the disc, which is hidden or uniform.
- 32: flip occurs on the lamp-0 tick, checked by a pin that samples the engine bar index against the lit segment on
  every step across at least two full passes.
- Rack review: dim contrast and placement at real brightness (human review, no automated gate).

OPEN
- Whether the 16-step disc is hidden or uniformly lit. Lean: hidden, to keep the 16-step panel identical to today.

CPU COMPARISON (before 64 is considered)
- Compare 16 vs 32 with everything else fixed: same patch, 16 active voices, per-voice advance on, same sample
  rate and buffer size. Record the optimisation branches included so each saving is visible.

════════════════════════════════════════════════════════════════════════════════
## DIMMING RULE — probability grid (Rodney, agreed)
Two independent display channels. Do not merge them.
- DISC (above) = which BAR is active (engine position).
- GRID DIMMING = which CELLS are inside the global window (the pattern's offset/length, displayed on the grid).

Rules
1. Patterns restricted to steps 1–16: the disc is hidden (always dimmed). Grid dimming is unchanged from today.
2. 32-step pattern, bar 1 (cells 1–16): cells outside the global window are dimmed.
3. 32-step pattern, bar 2 (cells 17–32): cells outside the global window are dimmed.
4. The global window is defined on the 32-step phrase. Each bar shows the intersection of the window with its own
   16 cells. A window that crosses bar 1 into bar 2 (or wraps past 32 back to 1) lights the in-window cells in both
   bars.
5. Dimming is display only. Playback and the engine window are unchanged. The grid reads the engine window and does
   not recompute it.

Gate (Stage 3)
- Pin: for a set of offset/length windows over 32 steps, the displayed in-window cell set equals the engine window
  set, per bar, including a window that crosses the bar boundary and one that wraps past 32.
- ≤16: grid dimming pixel-identical to today.

════════════════════════════════════════════════════════════════════════════════
## STAGE 2b — cap flip (kPatternLengthCap 16 → 32): REQUIRED PRE-GATES (from the 2a review)
Stage 2b is where 32-step behaviour turns on. It must not start until both gates below are in place and green
on the 2a tip. Both are added to Stage 2b's gate list.

### GATE 1 — non-default ≤16 equivalence pin (the mis-read detector)
Why: 2a's flat-stride bug was caught only because a mis-read happened to hit a cell, and the ≤16 goldens still
passed because default patterns compare equal across the mis-read. Default-valued patterns cannot detect index
errors, so the ≤16 gate is not yet a real guarantee.
What: a pin that runs the cap-16 build on patterns with VARIED probabilities and mask bits, and compares every
observable output against a recorded reference. Minimum coverage:
- per-step probability values varied across all 16 steps on every lane (rest, legato, accent, variation, qmix,
  melody, octave), not only the defaults;
- mask bits varied: a painted pattern with mixed on/off cells, plus an offset/length window;
- a voice-LOR window that crosses the 16-step boundary and wraps;
- at least one non-default subgate setting (ratchet/ghost REST, LEG, ADV, ACC).
Compare: note, gate, CV and display-state outputs step by step, against the reference recorded from the 2a tip
(cap 16, pre-flip). The reference is generated once, committed, and is the oracle for 2b.
Gate: zero differences at cap 16 after the flip. If the flip changes any output at ≤16, Stage 2b is wrong.
Note on the sweep: the flat-stride class is most likely to recur in cross-row reads, so include a case that reads
a neighbouring voice row and check it for the wrong value, not just the right one.

### GATE 2 — beyond-ring zero-fill pin
Why: the design depends on cells 16–31 of every per-step row staying zero while the cap is 16. Nothing currently
checks it, and a write leaking past the ring would corrupt the ≤16 output silently.
What: after a full run at cap 16 across varied patterns, assert every per-step row's cells at index ≥ kPatternLengthCap
are exactly zero, for every lane and every voice row, including the widened mask planes and the melody mirrors.
Gate: all beyond-ring cells zero at every sampled step. Run it at the end of the run and at intermediate steps,
since a leak can be transient.

### Stage 2b's own pins (unchanged, now run after the two gates above)
- wrap at 32; both bars reachable; bar indicator tracks the bar (see BAR INDICATOR);
- mask bits address all 32 steps;
- the draw-window cap flip widens DRAW_CHUNK and the Philox fill together, with its headroom math pinned.

### Order
1. Land GATE 1 and GATE 2 on the 2a tip, green at cap 16. Reference oracle committed.
2. Flip the cap (2b) as its own commit. GATE 1 must show zero differences at ≤16.
3. Add the 32-step pins.

════════════════════════════════════════════════════════════════════════════════
## LANTERN VIEW — DECIDED: one bar, follows the disc (Rodney)
- Lantern keeps its one-bar, 16-column view. It is observability only and writes no engine state.
- The view follows the bar indicator (the same engine bar index as the disc), with a manual cycle for inspection.
- Phrase-crossing notes are still visible: Lantern's existing held-over carets mark notes that continue across the bar
  boundary, so no 32-column view is needed for that.
- 32-column Lantern is deferred. Revisit it when the 64-step display is decided, since it has the same density cost.

REQUIRED BEFORE STAGE 2b (added to its gate list)
- Lantern's write path is currently bar-blind: `if (step < 0 || step >= 16) return` (and the matching row/step guards).
  With the cap at 32, steps 16–31 would be dropped silently. The write path must route each step to its bar slot
  (bar = step / 16, column = step % 16) so bar 2 is recorded while bar 1 is visible, and the cycle shows the chosen bar.
- Gate: a pin that drives a 32-step run and checks that Lantern records steps 16–31 in the bar-2 slot, and that the
  bar-1 view is unchanged at ≤16.

════════════════════════════════════════════════════════════════════════════════
## LANTERN — REVISED: always 16 columns (Rodney, tentative: "might always keep it at 16")
Reason: legibility, and usefulness for debugging as well as for the end user understanding the patch.
- Lantern stays a 16-column view at every pattern length. No 32-column mode.
- Open sub-question (decide before 2b): which 16 columns it shows on a 32-step pattern.
  Option A (lean): follow the disc. The view shows the active bar, and the existing held-over carets still mark notes
  that cross the bar boundary.
  Option B: a manual bar select only, with no auto-follow.
- Either option needs Lantern to keep both bars recorded (two 16-column buffers, or one 32-cell ring read as two bars).
  Otherwise switching to bar 1 after playing bar 2 loses bar 1's history.
- The bar-aware write path from the previous entry still applies: steps 16–31 must be recorded, not dropped.

════════════════════════════════════════════════════════════════════════════════
## LANTERN — DECIDED (Rodney)
- 16 columns at every pattern length.
- FOLLOWS THE DISC automatically: the displayed bar is the engine's active bar, the same index the disc shows.
- BOTH BARS RECORDED: steps 1–16 and 17–32 each keep their own 16-column history, so switching the view never loses
  the other bar's data.
- LATER (not now): a manual bar switch and/or a freeze-display option. Both are view-only and can be added without
  touching the recording path.

GATE (Stage 2b)
- Pin: a 32-step run records steps 16–31 into the bar-2 buffer and steps 0–15 into bar-1; the displayed bar equals the
  engine bar index on every step, including across the bar boundary.
- ≤16: Lantern output unchanged; the bar-1 buffer is the full history at 16.
