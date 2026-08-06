# Wiring the Micro tuning table into Monsoon (substituting for 12-TET) -- integration plan

How the Micro expander's per-degree faders (note PROBABILITY) + per-degree cents (TUNING) replace
Monsoon's built-in 12-TET pitch machinery, and the issues that arise. Post-library; part of the
microtonal arc (MICROTONAL_MASTER, MONSOON_MICRO_SPEC).

## The whole thing hinges on TWO seams in genPitchLive (PatternEngine.cpp:95)
The current 12-TET pitch generation is:
  (1) int sem = pickSemitone(in.semiWeights, r_semi);        // 12 fader weights -> a degree (0..11)
  (2) float v = float(oct) - 4.f + (sem + in.transpose)/12.f; // degree -> voltage, /12 = 12-TET
Rodney's two requirements map EXACTLY onto these two lines:
  - "faders set note probability" = generalise (1): N weights, not 12.
  - "CV quantized to expander settings" / tuning = generalise (2): cents-based voltage, not sem/12.
So the substitution is conceptually clean: swap the degree-count in (1) and the degree->voltage map in (2).

## The tuning table (the shared structure -- all modes read it)
Define ONE structure Monsoon holds, populated by the Micro when attached, else the built-in 12-TET:
  struct TuningTable {
    int   N;                 // degree count (12 built-in; 12 or 24 from Micro)
    float cents[MAXN];       // cents of each degree from the root (root = 0). MAXN = 24.
    float weight[MAXN];      // per-degree note PROBABILITY (the faders). 0 = disabled/skipped.
    // derived: cents[i]/1200 = octave-fraction for degree i's voltage.
  };
Built-in default: N=12, cents[i]=i*100, weight[i]=the existing 12 faders. Micro attached: N, cents[],
weight[] come from the expander. genPitchLive + quantize read this table instead of hardcoded 12.

## THE SEAMS TO CHANGE (every 12-TET hardcode found)
1. pickSemitone(weights[12], r) -> pickDegree(table.weight, table.N, r). Trivial: loop to N not 12.
   Returns a degree index 0..N-1. (PatternEngine.cpp:74)
2. genPitchLive voltage map (PatternEngine.cpp line ~117):
   OLD: v = oct - 4 + (sem + transpose)/12
   NEW: v = oct - 4 + (table.cents[degree]/1200) + transposeVolts
   i.e. degree -> voltage via its CENTS, not sem/12. (Transpose also needs cents-aware handling -- see
   issues.) 
3. rebuildScaleCache(weights[12]) -> take table.weight[0..N-1]. (SequencerEngine.cpp:326)
4. QUANTIZER modes C/D (SequencerEngine.cpp:957,968): 
   OLD: sem = round((pitchV - floor(pitchV)) * 12) % 12   // assumes 12 equal steps
   NEW: quantize inCV to the NEAREST ENABLED degree's cents in the table (see "quantize" below). This is
   the "CV quantized to expander settings" requirement. Must respect weight[i]==0 = disabled (skip).
5. quantize(inCV) helper: currently 12-TET nearest. Rewrite: find the degree (across octaves) whose
   cents value is nearest to inCV's octave-fraction, among ENABLED degrees; snap to it. Shared by C/D.
6. semiWeights / semiPlayRemain / markSemi / lastSemitone: these index by semitone 0..11 for the LED
   halo + "avoid repeat" logic. Generalise to 0..N-1 (N up to 24). The LED halo (12 positions) needs a
   display decision for N=24 (see issues).

## ISSUES THAT ARISE (the real work is here, not the seams)

### A. Semitone is an int index EVERYWHERE; N=24 breaks 12-sized arrays
sem/semitone is assumed 0..11 across the engine (semiWeights[12], the LED halo, markSemi, poly
melodySemitone, the Lantern colour-by-semitone). Widen all 12-sized note arrays to MAXN=24 and audit every
"% 12" and "* 12" and "[12]". This is the pervasive change -- like the PPQN 24/48/96 rework, a
find-every-hardcode job. WriteLedger/tests should guard it.

### B. Transpose in an UNEQUAL tuning -- RESOLVED by switching to OCTAVES (Rodney)
The problem: semitone-transpose adds (sem+transpose)/12 -- the /12 is 12-TET. In a custom tuning, shifting
by "1 semitone" moves OFF the tuning's grid (a 12-TET semitone isn't a degree of the tuning). Degree-shift
vs volts-shift was the open question.
RESOLUTION (Rodney): sidestep it -- use OCTAVE transpose only. An octave is +1.0V in ANY tuning (the
octave is always the octave; only its internal division varies). So octave-transpose is unambiguous and
tuning-native everywhere -- issue B DISSOLVES (no degree-vs-volts decision).
- INTERTROPICAL per-output transpose (the 8 knobs, currently -24..+24 SEMITONES, integer-detented,
  Intertropical.cpp:32-34, applied as transposeForOutput/12 at the roll/output): in MICRO mode -> switch
  to OCTAVES only (e.g. -3..+3 octaves, octave-detented, applied as whole volts). No /12, no off-tuning
  shift. This is SEPARATE from Monsoon's global in.transpose -- it's Intertropical's own 8 knobs.
- CENTS transpose: REJECTED (Rodney's doubt confirmed). A cents shift moves the sounding pitch BETWEEN
  the tuning's degrees -- off the tuning -- which defeats tuning-native generation. Cents-shift is a
  "global detune/calibration" concept, not "move the material". Skip it.
- [DECIDE LATER -- RODNEY]: make Intertropical's 8 knobs octaves-only in 12-TET mode TOO (consistency +
  issue B fully gone everywhere), OR keep semitones in 12-TET (preserves "harmonise output up a
  third/fifth", at the cost of transpose resolution CHANGING when a Micro attaches). Lean: OCTAVES-ONLY
  everywhere -- these knobs' job is ARRANGEMENT (spread 8 outputs across registers; octave-shift is the
  primary move); semitone harmony intervals are a secondary use better served by real pitch CV / a harmony
  tool than 8 detented knobs. Minor either way (Rodney: "its minor"). Decide when building Micro.
- Monsoon's GLOBAL in.transpose (genPitchLive, separate from Intertropical): if kept, same logic --
  octaves are safe in any tuning; a non-octave global transpose in a custom tuning needs the same
  octaves-only treatment or a degree-shift decision. Defer with the above; likely octaves-only in Micro.

### C. Octave wrap when N != 12
genPitchLive builds octaves by integer steps then adds sem/12. With N degrees, an "octave" is still 1V
(1V/oct standard preserved -- the tuning divides the octave into N, but the octave is still 1V). So
octave logic stays; only the within-octave fraction changes (cents/1200 instead of sem/12). BUT confirm
the octave range math (oL/oH from octaveLo/Hi) still works when degrees aren't equally spaced -- it should
(octave is octave), but verify the highest degree + octave doesn't exceed the +-5V clamp differently.

### D. NOTE-PLAYING DISPLAY at N=24 -- RESOLVED (no phantom "halo" -- it's the Lantern piano roll)
Earlier draft mentioned "the 12-LED halo" as a Monsoon panel element for showing the currently-playing
note. CORRECTION: Monsoon has no 12-LED note-pitch ring. What it HAS is a 16-LED STEP ring (STEP_LIGHTS
in Monsoon.hpp:413-414) that shows PLAYHEAD POSITION across the 16 sequencer steps -- not pitch. The
note-currently-playing indicator is the LANTERN piano roll, not a Monsoon halo. ("halo" in this codebase
is just a glow rendering effect on lit LEDs -- MonsoonWidget.cpp:94 -- not a named UI element.)
So the "12-LED halo vs N=24" question was a phantom. The real question -- how the note-playing display
handles N!=12 -- is already answered by the Lantern piano roll decision:
- 12-Micro: Lantern reuses the current 12-row keyboard piano roll (unchanged).
- N!=12 (24-Micro or arbitrary Scala): uniform N-row grid with degree-number labels, no keyboard graphic
  (see "LANTERN PIANO ROLL under a custom tuning" section above).
No separate halo issue -- it collapses into the piano-roll decision already made.

### E. Poly path duplication
genPitchLive is called mono (line 434) AND per-voice poly (758,881,911). All poly voices must read the
SAME tuning table. Single source of truth -- the table lives on the engine, all callers read it. No
per-voice tuning. (Confirm no poly path caches semiWeights[12] separately.)

### F. Delegation timing (when the Micro attaches/detaches mid-patch)
When a Micro attaches: swap the table (12-TET -> Micro's) at a SAFE point (block boundary, like other
expander wiring -- not mid-block). When it detaches: revert to the built-in 12 faders. The active notes
playing at swap time: let them finish; apply the new table to the NEXT note (read-at-boundary). Avoid a
pitch glitch on attach/detach.

### G. Scala .scl -> table population
The Micro reads .scl (cents per degree) -> table.cents[]. The faders set table.weight[]. Enable/disable =
weight 0. Save = write table.cents[] + which degrees enabled back to .scl. (.scl carries both -- role-
agnostic.) Rounding/precision: .scl cents are floats; store faithfully.

### H. Lantern / display note names in a custom tuning
The Lantern shows note names (C, C#, ...) -- meaningless in a 24-tone or non-12 tuning. Display decision:
show degree NUMBER (1..N) or cents, not note names, when a non-12-TET tuning is active. Ties to issue D.

## BUILD ORDER (incremental, testable)
1. Introduce TuningTable as the built-in 12-TET default (N=12, cents=i*100, weight=faders). Route
   genPitchLive + quantize + rebuildScaleCache THROUGH it. NO behaviour change (12-TET identical) --
   pure refactor, tests stay green. This is the safe foundation.
2. Widen all sem arrays 12 -> MAXN=24 + audit every %12/*12/[12] (issue A). Still 12-TET behaviour
   (N=12), tests green. The pervasive-but-inert change.
3. Generalise pickSemitone->pickDegree + the cents voltage map (seams 1,2) -- still N=12 so identical
   output; now N-capable.
4. Rewrite quantize for nearest-enabled-degree-by-cents (seams 4,5) -- verify Mode C/D still correct at
   N=12 (ties to the C/D pre-release pass).
5. Wire the Micro expander: populate the table from faders+cents+.scl; delegation + blanking (issue F);
   resolve transpose (B), halo (D), display (H) decisions.
6. Tests throughout: pitch-gen table test (assert cents->voltage), quantizer test (nearest enabled
   degree), N=12 regression (identical to old 12-TET), N=24 new cases.

## Decisions needed from Rodney (flagged inline)
- B: RESOLVED -- OCTAVE transpose (dissolves the unequal-tuning problem); cents rejected. Open sub-point:
  Intertropical 8 knobs octaves-only in 12-TET too (lean yes) or keep semitones in 12-TET? Minor, decide at build.
- D: RESOLVED (phantom -- there is no 12-LED halo; the note-playing display is the Lantern piano roll,
  already answered by the roll's 12-reuses / N!=12 uniform-grid decision).
- H: note-name display in custom tunings -> degree number / cents? (lean degree number)

## LANTERN PIANO ROLL under a custom tuning (issue H, expanded)
The piano roll's vertical axis is a LITERAL 12-tone keyboard: white backing + black keys at pitch
classes {1,3,6,8,10} + white-key dividers at E|F, B|C (Lantern.cpp:495-530), one row per semitone. That
black/white metaphor IS 12-TET and doesn't extend to other N.

### 12-Micro: REUSE the current piano roll as-is
A 12-tone Micro still has 12 degrees/octave -> the existing 12-row keyboard works. The kbm-style mapping
says which of the 12 keyboard rows each degree occupies; detuned cents don't change the ROWS (only the
exact pitch within, which the roll needn't show). So 12-Micro reuses everything. Clean.

### N != 12 (24-Micro, or any Scala N): UNIFORM N-ROW GRID, degree numbers, NO keyboard graphic
There is NO black/white keyboard for 24 (a piano is intrinsically 12). Options considered:
- (1) CHOSEN: drop the keyboard graphic for N!=12; show N uniform rows/octave with DEGREE-NUMBER labels
  (1..N) instead of note names. Honest for ANY tuning incl. arbitrary Scala. The piano becomes a plain grid.
- (2) REJECTED: "12 primary + 12 quarter-tone" keyboard scaffold -- bakes in the 12+quarter-tone
  interpretation, FALSE for true 24-EDO / arbitrary Scala. Same arbitrary-.scl reason we rejected the
  primary/inflection split for the PANEL (one row of 24) and must reject here too. CONSISTENT: 24-tone is
  "N arbitrary degrees", never "12 plus extras", from panel to roll to colour.
- (3) DEFERRED (later view option): CENTS-PROPORTIONAL rows -- each degree at its true cents height, so
  unequal tunings show real pitch distances (wide gaps / tight clusters = the tuning's actual shape). Most
  honest, biggest rewrite (non-uniform rows), harder to read as a time-sequence grid. Add later as an
  optional view once the uniform N-row grid works.

### ROW HEIGHT: keep it fixed; accept fewer octaves in view + more scrolling (DECIDED)
Do NOT shrink rows for 24 (thinner rows hurt note/content legibility). Keep the row-height constant; the
viewport just shows FEWER octaves (~5 oct at 12 rows -> ~2.5 oct at 24 rows, same height) and the user
scrolls more. Content legibility > viewport range. Only the octaves-in-view changes; the row-height
constant stays.

### COLOUR-BY-NOTE at N degrees: N-parameterised perceptual palette (not two hardcoded palettes)
Current: ~12 colours (one per semitone). At 24 you need 24 distinguishable colours -- but 24 mutually
distinct categorical colours is AT/PAST the human limit (people reliably distinguish ~8-12 categorical
colours; small panel + varied monitors make it worse). Decisions:
- The colour function must be N-PARAMETERISED: "give me N maximally-distinct colours", generated from N.
  Handles 12, 24, AND in-between (5-tone Slendro = 5 easy colours; 7-tone Pelog = 7; 24 = best-effort).
  One function, not a 12-palette + a 24-palette special-case.
- Use PERCEPTUAL spacing (OKLCH / CIELAB), NOT even hue steps: vary lightness + chroma too, so 24 colours
  are as distinguishable as possible. Even-hue 15-degree steps look alike between neighbours -> reject.
- REJECT the "12 hues x 2 light/dark shades" scheme -- it implies 12+12 primary/inflection structure,
  false for arbitrary Scala (same reason as everywhere). 
- REALISTIC EXPECTATION: even perceptually-spaced, 24 categorical colours won't read as instantly as 12.
  That's a perceptual ceiling, not a design failure. 
- OPTIONAL fallback view: a pitch-HEIGHT GRADIENT (low->high = spectrum sweep) as an alternative to
  categorical colour-by-note, for tunings where 24 categories confuse more than help. Offer both;
  categorical default, gradient optional.

### Note NAMES: degree numbers (1..N), not C/C#/... when N!=12 (issue H resolved)
Note names are 12-TET-only. For N!=12 show degree NUMBER (or optionally cents). For N=12 keep note names.

## COLOUR: only ENABLED degrees need colour (Rodney's insight -- dissolves the 24-colour ceiling)
KEY REALISATION: the tuning may have 24 degrees, but the SCALE (enabled degrees) is usually a small
subset -- a 5/7/9-note mode. Nobody enables all 24 and plays chromatically across a 24-tone tuning (not
musical). So you never actually need 24 distinguishable colours; you need N_active colours, and N_active
is typically ~5-9 = comfortably within the perceptual limit. Colour the SCALE, not the TUNING. The
24-colour ceiling only bites if all 24 are enabled, which is the rare edge case, not the default.

STATUS: decide-later. Options captured, no decision made yet.

### Option i -- colour assigned OVER the enabled subset (N_active-sized palette)
The N_active enabled degrees get colours 1..N_active from a palette sized to N_active. Always maximally
distinct (few colours, far apart). DOWNSIDE: a degree's colour CHANGES when you enable/disable others
(the subset re-colours) -- less stable while editing the scale.

### Option ii -- colour FIXED to tuning-degree position (24-wide palette, but only few shown)
Degree k always gets colour-k-of-N whether neighbours are enabled or not. STABLE (a note keeps its colour
as you toggle others). The 24-wide palette is theoretically hard to distinguish IN FULL, but you only
ever SHOW the enabled few at once -- and ~5-9 colours drawn from a 24-palette ARE legible even if the
full 24 aren't. So the perceptual ceiling becomes THEORETICAL (never seen all at once). Gets stability
AND practical legibility; ceiling dodged by scales-being-subsets.
  -> Rodney's insight makes this MORE attractive than it first seemed: fixed colours + subset-only-shown
     = stability + legibility, ceiling never actually bites.

### Edge: many degrees enabled (dense subset / all 24)
Rare. Either: palette does best-effort (less distinguishable, but rare so acceptable), OR auto-fallback to
the pitch-HEIGHT GRADIENT view above a threshold (e.g. N_active > 12) since categorical colour has stopped
being useful anyway. Decide-later.

### DECIDE LATER (all)
- (i) subset-assigned vs (ii) fixed-to-position. [lean (ii) given the insight: stability + only-few-shown]
- N_active-parameterised perceptual palette function (OKLCH/CIELAB) either way -- shared mechanism.
- Dense-subset fallback: best-effort vs auto-gradient-above-threshold.
- Whether "colour by note" (categorical) and "colour by pitch height" (gradient) are both offered as
  selectable views (earlier note) -- still open.
All post-library; part of the microtonal arc. Nothing here blocks the tuning-table refactor (build-order
step 1), which is colour-agnostic.

## COLOUR-BY-ACTIVE-DEGREE (Rodney's insight -- likely dissolves the 24-colour ceiling)
KEY INSIGHT (Rodney): with a 24-tone TUNING loaded, only a FEW degrees are usually ENABLED (the scale is
a subset -- a 5/7/9-note mode, rarely all 24). So you almost never need 24 distinguishable colours; you
need N_active colours, and N_active is typically small (well within the perceptual limit). Colour by
ACTIVE degree, palette sized to the enabled subset, not the whole tuning. The 24-colour ceiling only bites
if you enable ~all 24 -- which is rare/unmusical. So the hard case basically doesn't occur.

STATUS: DECIDE LATER. Options captured; not yet chosen.

### The two assignment models (decide later)
- FIXED-TO-TUNING-POSITION: degree i always gets colour i-of-N (stable -- a note keeps its colour as you
  toggle others). Palette must be defined N-wide (24), BUT since only the few ENABLED degrees are ever
  DISPLAYED, you never see all 24 at once -> the perceptual ceiling is only theoretical (showing 5 colours
  drawn from a 24-palette is legible even if the full 24 aren't mutually distinct). Gives stability AND
  legibility-in-practice; ceiling dodged by "scales are subsets".
- ASSIGNED-OVER-ENABLED-SUBSET: the N_active enabled degrees get colours 1..N_active from a palette sized
  to N_active (always maximally distinct, few + far apart). BUT a degree's colour CHANGES when you
  enable/disable others (the subset re-colours) -- less stable while editing.
Lean: FIXED-TO-POSITION now looks best (Rodney's insight makes the ceiling theoretical -- you only ever
display the enabled few, so a 24-wide palette is fine in practice, and you keep colour stability). But
DECIDE LATER.

### Edge: many degrees enabled (rare)
If someone enables a dense subset (>~12) categorical colour degrades. Graceful fallback (decide later):
either let the perceptual palette do its best, OR auto-switch to the pitch-GRADIENT view above a threshold
(N_active > ~12) since categorical colour has stopped helping anyway. The common case (few enabled) is
always easy; the rare dense case degrades gracefully.

### Mechanism (whichever model)
Colour function stays N-parameterised + perceptual (OKLCH/CIELAB), from the earlier section. Fixed-to-
position sizes it to N (tuning); assigned-over-subset sizes it to N_active (scale). Same function, different
N argument. Still reject the 12x2-shade scheme (false 12+12 structure).

### To decide later (summary)
1. Fixed-to-tuning-position vs assigned-over-enabled-subset (lean fixed-to-position).
2. Dense-subset fallback: best-effort palette vs auto-gradient above a threshold.
3. Whether colour-by-note default stays categorical with gradient optional, or gradient becomes default at
   high N_active.
All post-library, all deferred. The insight (scales are subsets -> few active colours) is the load-bearing
point; the exact model is a later call.

## INTERCHANGE MODULATION of Micro faders (Rodney's insight -- reuse, not duplicate)
Compositional principle applied again: Interchange already modulates Monsoon's 12 note faders. Extend by
reuse:
- Micro-12: SAME 12-fader shape as Monsoon's, just different owner. ONE Interchange can modulate Micro-12
  with essentially no Interchange change -- same target shape, different module. Free extension.
- Micro-24: 24 faders. Interchange knows 12. So PAIR TWO Interchanges -- one handling degrees 1-12, one
  handling 13-24 -- to cooperatively cover 24. No "Interchange-24" module needed; composition instead
  of duplication. Same "modes are entry points, behaviour is shared" principle: two Interchange entry
  points to a 24-fader bank, each doing its familiar 12-fader job.

### Fader labels for Micro-24: "C 1/13, C# 2/14, D 3/15..."
Each fader shows both interpretations: the note name it WOULD BE in 12-TET (equal-division default -- the
natural reference) + its degree number in the 24-tone system. Format "note-name degree-number/paired-
degree-number" (e.g. "C 1/13" for the fader at C position, degree 1, paired with degree 13 an equal-
tuning half-step higher). Users thinking in note names see them; users thinking in degrees see them;
nobody has to pick, and the "1/13" format visually connects the two-Interchange halves. Note names are
labels only -- meaningful for near-12-TET tunings, degrade to "just a reference" for arbitrary Scala
(where a "C" fader may sound nothing like C). Ties to issue H (note names in non-12 tunings): labels can
carry the 12-TET reference name PLUS the degree number, best of both.

### How two Interchanges attach to one Micro-24 (design question)
Each Interchange must know whether it's the 1-12 half or the 13-24 half. Options:
- (A) Position-based: first Interchange found = 1-12, second = 13-24. Simple, no config, BUT reordering
  the expander chain silently swaps which half each modulates. Confusing.
- (B) [LEAN] Explicit RANGE setting per Interchange (context menu): "Modulate: Monsoon 1-12" / "Micro-24
  degrees 1-12" / "Micro-24 degrees 13-24". User picks. Robust to reordering. Generalises the feature
  cleanly: Interchange targets ANY 12-degree bank, of which Monsoon/Micro-12/Micro-24-first/Micro-24-
  second are options.
- (C) Auto-assign but persistent: first-to-attach owns 1-12, second gets 13-24, remembers across chain
  moves. Clever but more state to track.
Lean B (explicit target selection). Reframes the feature as "Interchange can target any 12-degree bank"
rather than "Interchange knows about Micros specifically" -- cleaner architecture.
[DECIDE LATER -- RODNEY]

### One-Interchange-on-Micro-24 behaviour (graceful degradation)
If only ONE Interchange is attached to a Micro-24, modulate its assigned half (1-12 OR 13-24), leave the
other half unmodulated. Lets you incrementally add (attach one, hear it, decide if you want the second)
and never leaves the feature refusing to do something useful. Clean over restrictive.
[DECIDE LATER -- RODNEY, but lean graceful-degrade.]

### Two-Interchanges-on-Micro-12? (define this too)
Micro-12 has 12 faders. Attaching two Interchanges is redundant -- either they double-modulate the same
faders (competing writes, ambiguous) or the second is inert. Simplest: only ONE Interchange effective per
Micro-12; a second attached is inert (or refused). Ties to the single-writer discipline / WriteLedger.
[DECIDE LATER -- RODNEY, likely just "one active per Micro-12, second inert".]
