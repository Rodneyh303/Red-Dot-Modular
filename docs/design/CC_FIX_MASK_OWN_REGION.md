# CC FIX — the output gate uses its OWN region, NOT voiceLOR (Option A — Rodney confirmed)

Rodney confirmed: the LOR window = which probabilities the voice CYCLES ACROSS (the read — keep it). The
OUTPUT GATE was intended as a SEPARATE thing — a gate silencing output in global space, read untouched. The
bug is ONLY that MASK_DYNAMIC (Range) reads voiceLOR for its region. Everything else is already right.

## WHAT'S ALREADY CORRECT (no new state needed)
- The gate has its OWN store: maskPainted_[v]/maskAnchored_[v] (hpp:578/582) — 16-bit patterns per voice,
  SEPARATE from voiceLorStore_. The gate region already exists independent of the read window.
- Gate checks against the GLOBAL step (the prior fix). ✓
- PAINTED (abs2/abs3) ALREADY uses maskAnchored_ (hpp:640) — its own store, NOT voiceLOR. Painted is
  CORRECT (read full-phrase, gate cuts at the painted-mute global steps).
- Painting writes maskPainted_ (StraitsEastSandsVisual.cpp:362). "Set mask from range" seeds maskPainted_
  from windowMaskBits (:823-827).

## THE ONE BUG — MASK_DYNAMIC (Range) reads effectiveVoiceLor
SequencerEngine.hpp:637-638:
    if (kind == MASK_DYNAMIC) { effectiveVoiceLor(v, L, O, R, d); return maskWindowContains(O, L, phraseStep); }
This makes Range gate against the READ-confining voiceLOR window → the gate region = the read window →
playhead confined. WRONG: the gate must be independent of voiceLOR.

## THE FIX — Range becomes a CONTIGUOUS mask in the gate's own store
Drop the effectiveVoiceLor dependency from the gate entirely. Range = a contiguous region authored as a
MASK (the gate's own bits), gated against the GLOBAL step — exactly like Painted but contiguous:
- MASK_DYNAMIC resolves from the gate's OWN region (maskPainted_/maskAnchored_ as a contiguous run), NOT
  effectiveVoiceLor. The simplest: Range IS maskPainted_ seeded as a contiguous run (the existing "Set mask
  from range" seed), so Range and Painted share the one gate store — Range just constrains the UI to a
  contiguous edit. Then NOTHING in the gate reads voiceLOR.
- The read (laneTickV_/voiceLOR) is UNTOUCHED — the voice keeps cycling across its LOR window (its
  probabilities); the gate silences output where the global step is outside the gate region.
- The strip's Range authoring writes the GATE region (its own O/L → maskPainted_ bits), NOT voiceLorStore_.
  (If a dynamic O/L Range is wanted — drag resizes the gate region live — give the GATE its own O/L store,
  separate from voiceLorStore_; but the contiguous-maskPainted_ route reuses the existing store and is
  simpler. Prefer it unless Rodney wants live-dynamic gate geometry.)

## VERIFY
- Range: playhead traverses ALL 16 (read full, voiceLOR cycling unchanged); voice SILENT where the global
  step is outside the gate region, audible inside. Playhead NOT confined to the region.
- Painted: already correct — confirm still works (read full, cut at muted global cells).
- The read/probabilities the voice cycles across = the LOR window (unchanged, separate from the gate).
- Goldens: Cycle default bit-identical. GOLDEN_A untouched.

## NOTE
Two orthogonal things, now separated: READ window (voiceLOR — which probabilities the voice cycles across)
vs OUTPUT GATE (maskPainted_/maskAnchored_ — when the voice is audible, global space). The gate NEVER reads
voiceLOR. Painted already does this right; Range must too.
