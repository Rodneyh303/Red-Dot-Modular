# Changi terminal split + T1 mono bug (SCOPED, do in own session)

Rodney: Changi lacks step-gate and step-legato outputs. Solution = split into two terminals (NOT one
huge panel). Jewel theme parked -- the Rain Vortex can't be rendered well within nanosvg (no gradients/
filters/blur); prior attempts looked childlike. Keep Jewel as a name for later, only if a solid-fill
solution is found (the equatorial-line approach worked by ACCEPTING nanosvg constraints, not fighting).

## The family
- CHANGI T1 = current Changi, RENAMED + BUG-FIXED. 16 voices x (GATE + CV + ACCENT) = 48 jacks. The
  "departures" terminal: voice IDENTITY signals (what the voice is). 24HP airport-diagram panel.
- CHANGI T2 = NEW module. 16 voices x (STEP GATE + STEP LEGATO) = 32 jacks. The "articulation"
  terminal: phrasing signals (how the voice departs). ~16-20HP.
Split rationale: signal-type split maps to how you patch (all gates one place, all step-legato
another); T1/T2 = the real Changi terminals; 8-voice patches may need only T1.

## T1 MONO BUG (fix regardless of the split -- ships broken today)
MonsoonChangiExpander.hpp/.cpp mismatch:
- Comment (line 17) intends "index 0 = MONO (voice 1), 1..15 = poly voices 2..16" = 16 per type.
- BUT constructor configures only 15: `for (int i=0;i<15;++i) configOutput(... "Voice "+(i+2) ...)` --
  skips mono entirely, labels are voices 2..16 only.
- Widget (.cpp:33) binds 16: `for (int i=0;i<16;++i) bindOutput("output_gate_"+r, GATE_OUT_0+i)` --
  so output_{gate,cv,accent}_0 bind to UNCONFIGURED outputs. Jack 0 (mono) is bound but declared
  nowhere => broken/mono missing on the output side though the panel has the jack.
Fix: enum GATE/CV/ACCENT_OUT each need 16 (currently +15 => 16 entries, that's fine; the COUNT is
right, the CONFIG loop is wrong). Change constructor loop to `i<16`; label index 0 = "Mono (voice 1)",
indices 1..15 = "Voice 2".."Voice 16" (i+1). Verify the panel generator (gen_changi.py) already draws
16 jacks per band (it iterates the marker set) and that the data/message path actually carries the mono
strand to index 0. Confirm voice 1/mono value is available on the expander message (it leaves on the
parent normally -- Changi must also expose it here).

## T2 spec (new)
- Two output banks: STEP_GATE_0..15, STEP_LEGATO_0..15 (16 each incl. mono at index 0).
- Data source: the same expander-message poly voice state Lantern/Intertropical read -- step gate =
  the raw per-step gate pulse (POLY_STEP_GATE); step legato = POLY_STEP_LEGATO_GATE. (Straits output
  ids: GATE=0, STEP_GATE=1, STEP_LEGATO_GATE=2, CV=3, ACCENT=4 -- T2 exposes 1 and 2 per voice.)
- Passive widget like T1 (bindOutput to markers; no process()), unless the message needs unpacking.
- Panel: airport-terminal theme consistent with T1 (gen_changi_t2.py; reuse dotmod_design + the
  jack/apron idiom). ~32 jacks in 2 bands (STEP | STEP-LEG), 8 left / 8 right like T1's runway split.
- Discovery: same host/expander chain as T1 (findMonsoon side); place anywhere in the chain.

## Method / gotchas
- Enum growth shifts indices -> re-audit any positional bind loop and the message unpack.
- gen_changi.py is source-of-truth for geometry; widget reads markers. Keep that for T2.
- Commit T1 fix FIRST (it's a real bug), then build T2. Rack-verify mono jack lives on T1 and step/
  step-leg on T2.
- Slugs: if renaming Changi -> "Changi T1" changes the SLUG, that breaks patch compat post-library.
  Decide slug stability BEFORE library submission (keep internal slug "Changi", display name "Changi
  T1"? -- names can change freely, slugs cannot). FLAG for the library-submission checklist.
