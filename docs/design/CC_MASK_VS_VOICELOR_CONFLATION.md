# MASK vs VOICE-LOR — the conflation making the playhead cycle in-range (Rodney)

SYMPTOM: in Range AND Painted mode the voice PLAYHEAD cycles WITHIN the range. Per spec the read should run
FULL-PHRASE (cycle-mode everywhere); the mask is a PURE OUTPUT GATE against the GLOBAL step. The playhead
being confined is WRONG.

## THE CONFLATION (root cause)
MASK_DYNAMIC (Range) derives its region from effectiveVoiceLor (SequencerEngine.hpp:637-638:
`effectiveVoiceLor(v, L, O, R, d); return maskWindowContains(O, L, phraseStep)`). But voiceLOR is the
READ-CONFINING polymeter control (UNIFIED_ADDRESSING_SYSTEM.md:95-105: Length = "per-voice polymeter,
different cycles"; the read cycles within the voiceLOR window). So the voiceLOR is doing DOUBLE DUTY:
  (1) confines the READ (polymeter — the playhead cycles in-window). ← the symptom you see
  (2) defines the Range MASK region (= the same window).
These are TWO DIFFERENT, ORTHOGONAL features the spec keeps SEPARATE:
- voiceLOR = read-confining polymeter (TIME axis — different read windows). Read cycles in it. INTENDED.
- MASK = pure OUTPUT gate against the GLOBAL step; the read runs FULL PHRASE (spec:343 "cycle mappings run
  continuously, nothing internal changes; ONLY the output is gated"). SOUND REGIONS, not read-confinement.
So a Range/Painted mask must NOT confine the read — but because it's sourced from voiceLOR (which does), the
playhead is confined. The mask and the read-window are the SAME control; they must be SEPARATE.

## THE DESIGN DECISION (Rodney — which is the strip?)
The "voice range strip" currently sets voiceLOR (confines read) AND drives the mask (same O/L). Pick:
- OPTION A (likely intended): the STRIP authors the MASK region (output gate, global space); it does NOT
  confine the read. The read runs full-phrase (voiceLOR independent). MASK_DYNAMIC uses the STRIP's mask
  region, NOT effectiveVoiceLor. Polymeter (read-confining voiceLOR) is a SEPARATE control if wanted.
- OPTION B: the strip is voiceLOR (polymeter, confines read) AND a separate mask layer exists elsewhere.
RODNEY'S SYMPTOM (painting a range, expecting output-gating not read-confinement) points at OPTION A: the
strip = the MASK; the read should NOT be confined by it.

## THE FIX (for Option A — confirm first)
- MASK_DYNAMIC must source its region from the MASK's OWN O/L (the strip/mask store), NOT effectiveVoiceLor.
  Give the mask its own region (O/L or the painted bits), in GLOBAL-position space.
- The strip authoring must write the MASK region, NOT the read-confining voiceLOR — OR, if the strip is
  meant to be BOTH, the read must stay full-phrase (voiceLOR LEN = 16) while the mask gates the region.
- RESULT: read runs full 16 (playhead traverses the whole phrase); the mask silences output outside the
  region → "active only here" sound regions, playhead NOT confined.
- Painted: the painted pattern is the mask (global-space cells); read full-phrase, output cut at muted cells.

## VERIFY
- Range: playhead traverses ALL 16 steps (not confined); voice is SILENT outside [O,O+L), audible inside.
- Painted: playhead traverses all 16; voice cut at muted cells; the playhead VISIBLY crosses muted cells
  (silent there), does NOT cycle within the played cells.
- Polymeter (if kept as voiceLOR): a SEPARATE control; setting it confines the read (different feature).

## NOTE
The spec is explicit (343-348): reads run cycle-mode EVERYWHERE, ONLY output gated. A mask that confines the
read is the "earlier convolution... trying to make the READS special" the spec explicitly rejects. The mask
and the read-window (voiceLOR) are orthogonal: one gates output (global space, read full), one confines read
(polymeter). They must not be the same control.
