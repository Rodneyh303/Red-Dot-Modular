# CC PROMPT — dotted note values (build) — mirror triplet gating, PER-DOTTED toggles, mind the index shift

Design: DOTTED_NOTES_IDEA.md. Low-risk: add table entries + mask bits to the EXISTING varyNoteIndex gating,
exactly like triplets. Branch off lane-expander-refactor.

## 1. ADD 4 DOTTED to NOTE_VALUES (src/dsp/NoteValues.hpp) — NUM_NOTE_VALUES 8 → 12
constexpr NoteValue NOTE_VALUES[] = {fraction, allowedPPQN, label}, ordered SLOWEST → FASTEST; fraction×16 =
length in 1/16 steps. Insert the dotted values AT THEIR DURATION POSITION (keep slow→fast order):
- {0.75f,    …, "1/2." }  → 12 steps (integer)
- {0.375f,   …, "1/4." }  →  6 steps (integer)
- {0.1875f,  …, "1/8." }  →  3 steps (integer)
- {0.09375f, 4, "1/16."}  →  1.5 steps (FRACTIONAL → PPQN24 only, like triplets)
allowedPPQN: mirror the existing values (1/8=2|4, 1/16=2|4, triplets=4). Dotted-16th fractional → PPQN24(4)
only. Verify each fraction × resolution = integer pulses (vs SequencerEngine.cpp:734).

## 2. LEGATO — FREE (verified)
noteCanLeadLegato(idx) = "steps is an INTEGER". Dotted 1/2(12), 1/4(6), 1/8(3) integer → lead legato;
dotted-16th(1.5) fractional → cannot (like triplets 2.667/1.333 and 1/32=0.5). Existing logic handles it,
NO change.

## 3. THE CAREFUL BIT — the INDEX SHIFT
Inserting interleaved by duration SHIFTS existing indices, breaking the hardcoded magic indices in
varyNoteIndex allowed() (PatternEngine.cpp:329-331: idx==3→1/4T, idx==5→1/8T, idx==7→1/32). FIX: gate by a
straight/triplet/dotted FLAG on the NoteValue struct, not a magic index, so table order can change safely.
(At minimum re-derive the literals with comments tying each to its value.)

## 4. PER-DOTTED TOGGLES (Rodney — NOT a group; individual like triplets) — 4 bits, 4 menu entries
- noteVariationMask currently 0b111 (triplet bits 0,1,2; SequencerEngine.cpp:220). Add FOUR dotted bits
  3,4,5,6 — one per dotted value (NOT one group bit). Each dotted index in allowed() gated on its OWN bit,
  per-value exactly like the triplet gates.
- UI: the "Note Division and Clock" submenu (MonsoonWidget.cpp:1453-1459) already adds triplet toggles via
  add("Allow 1/4T",0); add("Allow 1/8T",1); add("Allow 1/32",2); — each XORs noteVariationMask bit `bit`,
  shows ✔ when set. Add FOUR dotted lines right after, SAME mechanism:
      add("Allow 1/2.", 3);
      add("Allow 1/4.", 4);
      add("Allow 1/8.", 5);
      add("Allow 1/16.", 6);
- DEFAULT: all 4 dotted bits OFF (SequencerEngine.cpp:220 stays 0b111) → existing patches byte-identical.
- BASE ALWAYS ACTIVE (existing `if (i==baseIdx) weights[i]=1` independent of allowed()): a dotted BASE note
  plays even with its dotted bit off = deterministic dotted base, the use case.

## 5. PANEL
NOTE VALUE dial: 4 more detents (12 total) — MonsoonWidget.cpp label mapping. (Dotted toggles done in §4.)

## 6. BLAST RADIUS (small) + PINS
- 4 files reference NUM_NOTE_VALUES: NoteValues.hpp, MonsoonWidget.cpp, MonsoonParameterManager.hpp,
  MonsoonConfigurator.cpp. Bump 8→12; verify each consumer handles 12 (loops, ranges, menu).
- GOLDENS BIT-IDENTICAL with all dotted bits default OFF (dotted is additive, off by default). If any golden
  moves, the index-shift broke an existing gate = the regression signal.
- NEW verified: each dotted base plays (bit off); each dotted variation reachable (its bit on); dotted-16th
  can't lead legato, dotted 1/2/1/4/1/8 can; PPQN allows each dotted correctly.

## DONE =
- 4 dotted in NOTE_VALUES (12 total, slow→fast), allowedPPQN correct, labels "1/2." etc.
- allowed() gated by straight/triplet/dotted flag (not magic index); FOUR dotted bits 3-6, FOUR per-dotted
  submenu toggles mirroring the triplet ones, all default OFF.
- Legato free (noteCanLeadLegato unchanged); dial 12 detents.
- Goldens bit-identical (dotted off by default); new dotted behaviours verified. run_all + strict green.

## NOTE — dotted-1/32 EXCLUDED (see PPQN_48_96_NOT_SAFE.md)
Dotted-1/32 needs 48 PPQN (4.5 pulses at 24, fractional). 48/96 are NOT safe today (the sequencer
double-speeds — the clock assumes external rate = ppqnSetting). So dotted stops at 1/16. Real 48/96 is a
separate clock feature; dotted-1/32 waits on it.
