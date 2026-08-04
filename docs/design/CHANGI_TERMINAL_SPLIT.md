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

## CHANGI T3 -- routes INTERTROPICAL (not Monsoon/Straits raw) -- the Lantern parallel
Insight (Rodney): T1/T2 break out the RAW 16-voice engine. But Intertropical produces an ARRANGED
8-channel output, and (exactly like Lantern) there's as much or more motivation to jack THAT out. So:
- T3 = 8 CHANNELS ONLY (Intertropical's slot budget <=8), each carrying ALL output types that T1+T2
  split across 16 voices: GATE + CV + ACCENT + STEP-GATE + STEP-LEGATO = 8 x 5 = 40 jacks.
- Organized BY CHANNEL (ch1's gate/CV/accent/step/step-leg adjacent), not by signal type -- because
  when patching an arranged output you send one channel to one synth voice, so its signals want to be
  together. This is why all 5 types fit one module here but needed 2 terminals for 16 raw voices.
- CV is POST-TRANSPOSE, tie-latched: read Intertropical's effectiveTranspose[ch] (the SAME field
  Lantern's piano-roll reads), so T3 jacks == Lantern display == what Intertropical actually outputs.
  One source of truth (Intertropical audio owns the latch; T3 and Lantern both mirror it).
- ASSOCIATED with a SPECIFIC Intertropical, like Lantern pairs with one. ALLOW MULTIPLE: one T3 per
  Intertropical.

### T3 shares Lantern's pairing problem -> shared solution
"Which Intertropical am I bound to" is the SAME discovery/pairing challenge Lantern has (findIntertropical
is fine for ONE pair, ambiguous with several). Both want the numbered PAIRING SYSTEM (Lantern
self-assigns a number, Intertropical picks, pair colours on both). => Build the pairing system ONCE,
generally, and have BOTH Lantern and Changi T3 consume it. Do NOT build a T3-specific binding.

### The loop this closes
Intertropical ARRANGES (voice->slot->output, transpose) -> Lantern VISUALISES the arrangement (eyes)
-> Changi T3 JACKS OUT the arrangement (cables). One source, two consumers, one pairing mechanism.
Data path for T3 = the same routed output T3 reads via voiceForOutput(scene,ch) + effectiveTranspose,
OR directly off Intertropical's output jacks (outputs[GATE/CV/ACCENT...].getVoltage(ch)) -- decide
engine-read vs jack-read like the Lantern product/debug split (T3 is a JACK breakout so reading
Intertropical's actual output voltages is arguably the RIGHT call here, unlike Lantern which needs
engine detail for colour/note-type).

### T3 DATA SOURCE -- SETTLED (host-pushed, mirroring how Changi actually works)
Checked how Changi gets its data. ANSWER: Changi's own process() is EMPTY. The host Monsoon's
MonsoonOutputGenerator reads the ENGINE directly (engine.gs, engine.voices[i].gs.currentPitchV,
engine.lastStepResult.accented) and PUSHES into cachedChangiExpander->outputs[...].setVoltage(...) per
sample. Changi is a passive jack-holder; the HOST writes into it.

So "T3 is to Intertropical as T1/T2 are to Monsoon/Straits" resolves the source question DEFINITIVELY,
and OPPOSITE to the earlier tentative jack-read lean: T1/T2 do NOT read jacks -- the host writes into
them from engine state. The faithful mirror:
- INTERTROPICAL's process() writes into T3's output ports, exactly as Monsoon's OutputGenerator writes
  into Changi's. T3 = passive jack-holder (empty process(), just bindOutput + a cachedT3 pointer).
- Intertropical already computes the routed per-output state (voiceForOutput scene->slot->voice, the
  gate/cv/accent/step/step-leg it reads from Straits, and effectiveTranspose[] tie-latched). It reaches
  into cachedChangiT3->outputs[...] and fills them from that -- for its 8 arranged channels.
- => T3 naturally carries POST-transpose, TIE-LATCHED values because Intertropical resolves them in its
  process; T3 just receives what's already computed. No jack-reading, no provenance loss, architecturally
  identical to T1/T2. (SUPERSEDES the "likely JACK-read" guess earlier in this doc.)
- Mechanism: Intertropical caches an adjacent T3 (expander scan, like cachedChangiExpander) and writes
  its 8ch x {gate,cv,accent,step-gate,step-leg} into T3's ports each block.

### T1 SLUG (library-critical decision -- SETTLE before submission)
Renaming Changi -> "Changi T1": the display NAME can change freely, but the SLUG cannot post-library
(breaks patch compat). DECISION: keep the internal slug as-is (the current Changi slug), set only the
display name to "Changi T1". New modules T2/T3 get fresh slugs. Existing patches keep working and the
browser still shows the T1/T2/T3 family. If not yet published, the slug is free -- but decide NOW and
freeze it.

## Method / gotchas
- Enum growth shifts indices -> re-audit any positional bind loop and the message unpack.
- gen_changi.py is source-of-truth for geometry; widget reads markers. Keep that for T2.
- Commit T1 fix FIRST (it's a real bug), then build T2. Rack-verify mono jack lives on T1 and step/
  step-leg on T2.
- Slugs: if renaming Changi -> "Changi T1" changes the SLUG, that breaks patch compat post-library.
  Decide slug stability BEFORE library submission (keep internal slug "Changi", display name "Changi
  T1"? -- names can change freely, slugs cannot). FLAG for the library-submission checklist.
