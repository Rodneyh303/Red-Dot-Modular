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

- **Ring:** embed_monsoon.py:32–33 — 16 `STEP_LIGHT_{i}` anchors at r=14 → 32 anchors (enlarged radius
  per LIGHT_RING_32_STEPS: position-only, no pattern-detail role, red-quarter/green-rest kept;
  FIXED master-step scale per D8 — NOT twice-around-16). Monsoon.hpp:500–503 `STEP_LIGHTS_END =
  START + 16` → `+ 32` (offset-based enum re-seats the downstream ids automatically).
  MonsoonWidget.cpp:474–475 bind loop 16 → 32; the `i % 4` red rule is MUSICAL (quarters at the
  clock's 16th resolution) — unchanged under D6 option (a), or → `i % 8` under option (b).
- **Ring feed (the audit's find — NOT in the first inventory):** Monsoon.cpp:1461
  `float stepBrightness[16]` + :1462 the fill loop + :1492 `updateStepLights(stepBrightness, 16)`;
  MonsoonUIManager.cpp:100 `for (int i = 0; i < 16 && i < count; ++i)` — all → MAX_STEPS. A missed
  site here leaves lamps 16–31 permanently dark (the upper half of the D8 ring dead) while the
  engine plays them. The playhead comparison itself (`lightIdx == stepIndex`,
  SequencerEngine.cpp:874) is frame-generic AS WRITTEN — it extends mechanically BECAUSE the ring
  is 32; twice-around-16 would have forced a NEW `% 16` into this path (`stepIndex % 16`), the
  exact landing class P4 sweeps out (→ D8).
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
- **P5 — the surfaces:** ring SVGs (32 lamps, enlarged, position-only, FIXED master-step scale per
  D8), STEP_LIGHTS enum + widget bind + the ring feed (the F find), Sands stepW, Lantern N_STEPS
  (F). Anchor audit 9/9.
- **P6 — pins:** new suite test_steps_32: len-32 march vs the closed forms (all four shapes — 32 is a
  power of two, the congruence laws' easy case); warm 16↔32 mid-run transitions == f(t, config);
  D3 old-vs-new window pins (offset 15 + len 16, documented); legacy-json load (16-wide keys → [0..15],
  play identical); mask hi/lo roundtrip; ring anchor count (32; `i % 4` → 8 red under D6(a)).
  Digest re-capture list per THE GOLDEN LAW.

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
- **D6 — anchor marking at 32 (from Rodney's resolution note: "32 steps is 1 bar at 32nd notes"):**
  the ring is resolution-agnostic — 2 bars of 16ths (the clock's native step) and 1 bar of 32nds are
  the SAME 32-step frame; no DSP site depends on the reading. The reading decides presentational
  marking only: **(a)** honest-to-clock — steps ARE 16ths, so red quarters every 4 steps (8 red
  anchors) + a bar tick at 16 (recommended: the anchors keep marking the clock's actual quarters;
  the `i % 4` widget rule survives; the two-bar reading matches how the long-note gain was argued
  in STEP_COUNT_DECISION — whole notes fit twice); or **(b)** 1-bar-of-32nds — red every 8, no
  mid-ring bar tick (the single-bar identity; the anchors stop marking the clock's quarters). Zero
  DSP impact either way.
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

## Sequencing

Own branch off the lane tip AFTER feat/position-holders lands (never ride the same train — this touches
the copula/CA/reversibility/hot-path frame the position-holder work just stabilized). P0 lands first as
the reviewable no-op; the resize commit is P1–P4; surfaces and pins follow. The battery is the safety
net at every gate, same discipline as the position-holder refactor.
