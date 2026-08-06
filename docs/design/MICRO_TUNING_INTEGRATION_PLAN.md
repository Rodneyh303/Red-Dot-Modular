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

### B. Transpose in an UNEQUAL tuning is NOT sem+n (SCALES_AND_QUANTIZER already flagged this)
OLD transpose adds n semitones (sem+transpose)/12 -- valid only in equal tuning. In an unequal tuning,
"transpose by 1 degree" shifts by a DIFFERENT cents amount at each degree. DECISION NEEDED: is Monsoon's
transpose (a) degree-shift (move n degrees along the table -- intervals vary, "modal" transpose) or (b)
volts-shift (add a fixed voltage -- constant pitch shift, may leave the tuning)? Likely (a) degree-shift
for musical coherence, but it changes what transpose "means". Flag for Rodney.

### C. Octave wrap when N != 12
genPitchLive builds octaves by integer steps then adds sem/12. With N degrees, an "octave" is still 1V
(1V/oct standard preserved -- the tuning divides the octave into N, but the octave is still 1V). So
octave logic stays; only the within-octave fraction changes (cents/1200 instead of sem/12). BUT confirm
the octave range math (oL/oH from octaveLo/Hi) still works when degrees aren't equally spaced -- it should
(octave is octave), but verify the highest degree + octave doesn't exceed the +-5V clamp differently.

### D. The LED halo (12 physical positions) vs N=24
The Monsoon panel's note halo has 12 LED positions (the 12 semitones). With a 24-tone tuning there are 24
degrees but 12 LEDs. DECISION NEEDED: (a) halo shows only the 12 "primary" degrees (loses the quarter-tone
degrees -- bad for pure 24-EDO), (b) halo repurposed / the Micro's own faders ARE the display (Monsoon
halo blanks in 24 mode, like the faders blank), (c) halo shows nearest-12 approximation. Likely (b) --
when a Micro is attached the DISPLAY delegates to the expander too (consistent with faders blanking).
Flag for Rodney. (12-tone Micro: halo still works 1:1.)

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
- B: transpose = degree-shift or volts-shift in unequal tunings? (lean degree-shift)
- D: 12-LED halo behaviour at N=24? (lean: display delegates to expander, halo blanks)
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
