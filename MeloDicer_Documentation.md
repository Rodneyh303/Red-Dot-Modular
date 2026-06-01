# Monsoon Documentation

## Overview

**Monsoon** is a stochastic melodic sequencer for VCV Rack, inspired by the Vermona meloDICER hardware sequencer. It combines rhythm, pitch, octave, and expression probability to generate evolving, expressive melodic sequences with deep parameter control and modulation capabilities.

The module generates 16-step patterns where each step's rhythm, pitch, octave, legato behavior, rest probability, and accent status are independently controlled by probabilistic DNA strands. Poly voice expansion (via **Straits**) enables up to 7 additional voices that follow the mono gate while drawing independent pitches.

---

## Quick Start

### Basic Setup

1. **Clock In**: Connect a clock signal (typically 16 PPQN for 16th-note resolution)
2. **CV Out**: Patch to a VCO's pitch input (1V/octave)
3. **Gate Out**: Patch to an envelope generator or VCA
4. **Set Octave Range**: Use OCTAVE LO and OCTAVE HI sliders to define the pitch range

### Default Behavior

With default settings:
- **Mode**: A (Clock-driven)
- **Note Length**: 1/8 (eighth notes)
- **Rest Probability**: ~10%
- **Legato Probability**: ~10%
- **Octave Range**: 2–5 (C2 to C5)

The module generates a 16-step pattern that loops and evolves based on the DNA strand windowing.

---

## Front Panel Controls & I/O

### Main Panel

#### Inputs
- **CLK IN**: Clock input (16 PPQN = 16th notes)
- **GATE1 IN** (Mode B): External gate/trigger for step advancement
- **GATE2 IN** (Mode C): Reserved for future mode expansion
- **CV1 IN**: Contextual CV (varies by mode: transpose, octave offset, scale modulation)
- **CV2 IN**: Contextual CV (varies by mode)
- **ACCENT CV IN**: External modulation of accent gate probability (0–10V = 0–100% accent)
- **RUN GATE IN**: Start/stop the sequencer when high
- **RESET TRIGGER IN**: Reset playhead to step 0 on rising edge
- **SEED IN**: Regenerate patterns (melody, rhythm, variation) on rising edge
- **LENGTH IN**: CV modulation of phrase length (0–10V = 1–16 steps)
- **OFFSET IN**: CV modulation of phrase offset window

#### Outputs
- **GATE OUT**: Main mono gate (10V when note sounds, 0V when silent)
- **CV OUT**: 1V/octave pitch voltage
- **TIE OUT**: High (10V) when mono decision is "Tie" (extend same pitch)
- **LEGATO OUT**: High (10V) when mono decision is "Legato" or "LegatoMax" (slide without retrigger)
- **ACCENT OUT**: High (10V) when note is accented and gate is open
- **SEED OUTPUT**: Diagnostic output reflecting current melody + rhythm seed as voltage (0–10V)
- **RESET TRIGGER OUT**: Trigger output when phrase boundary is crossed
- **RUN GATE OUT**: Mirror of internal run state

#### Main Knobs & Sliders

**Mode Select** (left side, 4-way)
- **A**: Clock-driven (external clock required; advances on 16th-note edge)
- **B**: Gate-driven (external gate triggers pattern; useful for synchronized launches)
- **C**: Reserved for future expansion
- **D**: Reserved for future expansion

**Pattern Length Controls** (top row, left to right)
- **NOTE VALUE PARAM**: Select note length grid (1/16, 1/16T, 1/8T, 1/8, 1/8D, 1/4T, 1/4, 1/2)
  - Affects the duration each step "holds" before moving to the next
  - Triplet and dotted values create rhythmic variation
- **VARIATION KNOB**: Probability bias (0–1)
  - 0.5 = no variation (all steps use base note length)
  - < 0.5 = bias toward longer notes
  - \> 0.5 = bias toward shorter notes
- **LEGATO KNOB**: Probability of legato/tie behavior (0–1)
  - Controls likelihood of slides (legato) vs. retriggered notes
  - At 1.0: always legato (every note slides, no retriggers)
- **REST KNOB**: Probability of rest (silence) on each step (0–1)
  - 0 = no rests (always plays)
  - 0.5 = 50% chance of rest each step
  - 1.0 = always silent
- **ACCENT KNOB**: Probability of accent gate on new notes (0–1)
  - Controls how often the ACCENT OUT fires
  - Only applies to NewNote, Legato, and LegatoMax decisions
- **TRANSPOSE KNOB**: Semitone transpose (-12 to +12)
  - Shifts all pitches up or down by the selected semitones

**Octave Controls** (bottom row, left to right)
- **OCTAVE LO SLIDER**: Lowest octave in playable range (0–8, default 2)
  - Maps to voltage: (octave − 4) V; e.g., octave 2 = −2V = C2
- **OCTAVE HI SLIDER**: Highest octave in playable range (0–8, default 5)
  - Pitches are randomly distributed between Lo and Hi each step
- **LOCK BUTTON**: Lock the octave sliders (prevent accidental changes during performance)
  - LED indicates lock state
- **MUTE BUTTON**: Silence the gate output without stopping the sequencer
  - LED indicates mute state

#### Pattern Analysis LEDs

**Step Indicator Lights** (16 lights across the top)
- Display the current playhead position and pattern rhythm activity
- Brightness indicates step probability and activity

**Dice LEDs** (below pattern lights)
- **RHYTHM DICE**: Shows whether the current step is a "rest" or "play" in the rhythm strand
- **MELODY DICE**: Indicates which semitone is selected for the current step
- **LOCK/MUTE/MODE LEDS**: Status indicators for lock, mute, and current mode

---

## Interchange (Modulation Expander)

The Interchange expander provides **per-semitone pitch modulation** and **dynamic octave range control**, allowing deep real-time parameter manipulation from external CV sources.

### What It Does

This left-side expander adds 14 CV inputs and 14 attenuverters to control:
- **Semitone Selection** (12 CV channels): Modulate the availability/strength of each note in the scale
- **Octave Range** (2 CV channels): Dynamically shift the playable octave bounds

### Interchange Controls

#### Per-Semitone Modulation (12 sets)

For each semitone (1 through 12):

**CV Input** (0–10V range)
- External CV source for this semitone
- Adds to the semitone's base probability from the melody strand

**Attenuverter** (−1 to +1)
- **−1**: Inverted (negative) response to CV
- **0**: CV disconnected (no modulation)
- **+1**: Normal (positive) response to CV
- Scales the incoming CV before applying to semitone

**How It Works**:
The module adds: `semitone_offset = (CV_input × attenuverter_value) / 10.0`

This shifts the pitch output for selected notes, creating:
- Filter sweep effects (CV from filter LFO)
- Sidechain modulation (CV from compressor)
- Expression control (CV from expression pedal)
- Polyrhythmic pitch variations

#### Octave Range Modulation (2 channels)

**Octave Lo CV Input & Attenuverter**
- Modulate the **OCTAVE LO** slider in real-time
- Range: −8 to +8 octaves of offset (attenuverter × input voltage × 0.8)
- Use to: sweep the lowest playable octave up and down

**Octave Hi CV Input & Attenuverter**
- Modulate the **OCTAVE HI** slider in real-time
- Allows shrinking/expanding the octave range dynamically
- Use to: create filtering effects or performance variations

### Interchange Patching Examples

**Melodic Sidechain Expression**:
```
Filter LFO OUT → Semitone 1 CV IN (to emphasize root notes)
Filter LFO OUT → Semitone 5 CV IN (to emphasize major 4ths)
Attenuverters: +0.5 (moderate CV response)

Result: Root and 4th notes become more prominent when filter LFO is high
```

**Dynamic Octave Sweep**:
```
LFO (0.1 Hz, 0–5V) → Octave Hi CV IN
Attenuverter: +1.0

Result: Octave range slowly expands and contracts, creating a wave-like effect
```

**Expression Pedal Modulation**:
```
Expression Pedal (0–10V) → Semitone 7 CV IN (dominant 7th)
Attenuverter: +0.8

Result: Expression pedal intensity controls the likelihood of dominant notes
```

### Modulation Tips

- **Start with 0 attenuverter**: Safely dial in CV mapping without audible changes
- **Invert interesting modulations**: Negative attenuverters create inverse effects
- **Modulate octave range**: Combine with sequencer LFOs for evolving spectral effects
- **Quantize CV sources**: Use quantizer modules upstream for melodic coherence

---

## Sands (DNA Expander)

The Sands expander provides deep control over the **six independent DNA strands** that drive the sequencer.

### What Are DNA Strands?

Each strand is a 16-element array of random probabilities (0–1) that controls a different aspect of the melody:

1. **Rhythm Strand**: Controls when steps rest vs. play
2. **Variation Strand**: Biases note length selection
3. **Legato Strand**: Controls legato/tie probability
4. **Accent Strand**: Controls accent gate probability *(NEW)*
5. **Melody Strand**: Selects which semitone to play
6. **Octave Strand**: Selects which octave to use

Each strand can be **windowed** (subset of 16 steps) and **rotated** (shifted) to create polyrhythmic and polymorphic variations.

### Sands Controls

#### Per-Strand Parameters (6 sets, one for each DNA strand)

For each strand (Rhythm, Variation, Legato, Accent, Melody, Octave):

**Length Knob** (0–15, default 16)
- Defines the active window size for the strand
- Value 16 = use all 16 steps; 8 = use only steps 0–7 (repeat every 8 steps)
- Creates polyrhythmic patterns when different strands have different lengths

**Offset Knob** (0–15)
- Shifts which steps are included in the window
- Offset 0 = start at step 0
- Offset 8 = start at step 8 (skip first half of pattern)
- Useful for creating call-and-response patterns

**Rotation Knob** (0–15)
- Rotates the strand values cyclically
- Rotation 1 = rotate by 1 step (last element moves to front)
- Allows generating variations of the same pattern
- Useful for melodic transformations

**Length CV Input** (0–10V)
- Modulate the length parameter with external CV
- 0V = length 0; 10V = length 15
- Enables dynamic pattern changes in response to other modules

**Offset CV Input** (0–10V)
- Modulate the offset parameter with external CV
- Creates shifting window effects

#### Scramble Buttons (6 total)

- **SCRAMBLE ALL**: Regenerate all six strands with new random values
- **SCRAMBLE [STRAND]**: Regenerate only the selected strand
- Each button has an associated **Gate Input** for external triggering (useful for synchronized scrambles)

#### Reset Buttons (6 total)

- **RESET ALL**: Restore all strands to their original (pre-scramble) values
- **RESET [STRAND]**: Restore only the selected strand
- Each button has an associated **Gate Input** for synchronized reset

### DNA Strand Example

**Scenario**: You want a pattern that plays most of the time but has occasional rests in a 4-step cycle.

1. Set **Rhythm Strand Length** = 4 (cycle every 4 steps)
2. Set **Rhythm Strand Offset** = 0
3. Use **Scramble Rhythm** until you get a pattern like `[0.1, 0.05, 0.15, 0.8]`
   - Steps 0–2 have low rest probability (mostly plays)
   - Step 3 has high rest probability (often rests)
4. Now the rhythm repeats every 4 steps with one syncopated rest

---

## Straits (Poly Voice Expander)

The Straits expander adds **7 additional voices** (for 8 total) that follow the mono gate while drawing independent pitches from the melody strand.

### Poly Voice Concept

- **Mono voice** (main module): Determines rhythm (rest/play/tie/legato) and is the "master"
- **Poly voices** (expander): Inherit the mono gate timing but:
  - Draw independent pitches from the melody strand
  - Each voice has independent rest probability
  - Can play different notes simultaneously (unison, chords, etc.)

### How Poly Voices Follow Mono

1. **NewNote/Legato/LegatoMax**: Poly voices can independently decide to play a new note or rest
2. **Rest**: Poly voices cannot start a new note, but already-sounding notes continue to decay naturally
3. **Tie**: Poly voices that are sounding extend their held note; silent voices stay silent
4. **MidNote**: Poly voices simply tick their hold timers, no new note decisions

### Straits I/O

#### Per-Voice Outputs (7 voices, labeled 2–8)

For each voice (Voice 2 through Voice 8):

- **Gate Output**: 10V when the voice is sounding, 0V when silent
- **CV Output**: 1V/octave pitch (independent from other voices)
- **Accent Output** *(NEW)*: 10V when the voice is accented and sounding, 0V otherwise

#### Voice Rest Probability

Each voice has an independent **Rest Probability Parameter** (set from the Sands expander or via Rack context menu). This controls whether a given voice plays when the mono decision allows it.

**Example**: 
- Mono decides to play a new note
- Voice 2 has 20% rest prob → 80% chance it plays
- Voice 3 has 60% rest prob → 40% chance it plays
- Voice 4 has 0% rest prob → always plays
- Result: unpredictable unison/harmony each step

### Poly Voice Timing

- **Gate rise**: Synchronized with mono gate
- **Pitch draw**: Independent each voice, but from the same melody/octave DNA strands
- **Gate fall**: Each voice's hold decays naturally; NOT synchronized with mono

This ensures poly voices sound like independent players following a conductor (mono) rather than exact clones.

---

## Accent Gates *(NEW FEATURE)*

Accent gates add dynamic emphasis to your sequences, useful for:
- Dynamics and expression (send to VCA before final output)
- Drum programming (trigger snare/hi-hat accents)
- Sequencing synthesizer parameters (filter cutoff on accented notes)

### How Accents Work

1. **Accent Probability Knob**: Sets the base probability (0–1) that a note is accented
2. **Accent CV Input**: Modulate the probability in real-time (0–10V = 0–100%)
3. **DNA Accent Strand**: Each of 16 steps has independent accent probability (can be windowed and rotated)
4. **Decision Logic**: Accent is only applied to **NewNote, Legato, and LegatoMax** decisions
   - Rest steps: never accented
   - Tie steps: never accented (extending the same note)
   - MidNote: never accented (just continuing to hold)

### Accent Outputs

- **ACCENT OUT** (main module): 10V when mono voice is accented and gate is open
- **POLY ACCENT OUT 1–7** (poly expander): 10V when each poly voice is accented and its gate is open

### Accent Examples

**Dynamic Expression**:
```
Patch: CV OUT → VCO
       GATE OUT → Envelope
       ACCENT OUT → VCA (before mixer)
       
Result: Accented notes have higher amplitude
```

**Drum Snare Accents**:
```
Patch: CV OUT → Pitch Sequencer (off-topic module)
       GATE OUT → Kick Drum Synth
       ACCENT OUT → Snare Synth
       
Result: Kick plays every note; snare only on accented beats
```

**Filter Modulation**:
```
Patch: CV OUT → VCO
       GATE OUT → Envelope
       ACCENT OUT → Filter Cutoff CV Input (via attenuator)
       
Result: Accented notes have brighter timbre
```

---

## Operating Modes

### Mode A: Clock-Driven

- **Trigger**: Advances on every clock pulse (16 PPQN = 16th-note steps)
- **Use Case**: When an external sequencer or clock provides steady timing
- **Best For**: Synchronized polyrhythmic patterns with other modules

**Control**: 
- Mono gate timing follows the clock
- Poly voices tick in sync with mono

### Mode B: Gate-Driven (Gated Step Sequencer)

- **Trigger**: Advances on rising edge of GATE1 IN
- **Use Case**: Perform manual sequencing or gate-driven patterns
- **Best For**: Live performance, hand-controlled stepping, integration with other gate sources

**Control**:
- GATE1 IN controls when steps advance
- Hold gate high for multiple clock cycles and the step won't change until gate releases
- Useful for exploring variations slowly

### Mode C & D

Reserved for future expansion.

---

## Parameter Tips & Advanced Techniques

### Creating Polyrhythmic Patterns

Polyrhythms emerge naturally when DNA strands have different lengths:

1. Set **Rhythm Strand Length** = 4 (4-step cycle)
2. Set **Melody Strand Length** = 6 (6-step cycle)
3. Set **Legato Strand Length** = 8 (8-step cycle)

The pattern repeats every LCM(4, 6, 8) = 24 steps. This creates complex, evolving patterns.

### Using CV1 & CV2

**CV1 Behavior** (context-dependent):
- **Default**: Transpose (same as TRANSPOSE knob)
- **Custom Modes**: Can be patched to affect octave offsets or other parameters

**CV2 Behavior**:
- Typically used for expression/dynamics in standalone patches

### Seeding & Regeneration

- **SEED INPUT**: Trigger to regenerate patterns
- **SEED OUTPUT**: Diagnostic voltage showing current seed state
- Patching SEED OUT back to SEED IN creates feedback loops (useful for evolution)

### Rest Probability Priority

The sequencer respects **fractional note tails** (e.g., triplets) when deciding whether to rest:

- If a note has a fractional tail (e.g., a 1/4T note played with a 1/8 grid), and a rest is drawn for the next step, the rest is **prevented** until the note tail finishes
- This ensures rhythmic integrity and avoids choked-off notes

---

## Outputs Reference

| Output | Voltage Range | Notes |
|--------|---------------|-------|
| GATE OUT | 0–10V | Standard gate; includes 1ms pulse for retrigger |
| CV OUT | 0–5V | 1V/octave; clamped to prevent extreme values |
| TIE OUT | 0 or 10V | Tie decision indicator |
| LEGATO OUT | 0 or 10V | Legato/LegatoMax indicator |
| ACCENT OUT | 0 or 10V | Accent gate; only high when accented AND gateV > 5V |
| SEED OUTPUT | 0–10V | Diagnostic; reflects seed state |
| RESET TRIGGER OUT | Gate pulse | Rising edge at phrase boundary |
| RUN GATE OUT | 0–10V | Mirrors internal run state |

---

## Patching Examples

### Basic Melodic Sequencing

```
CLK IN ← Clock (from Bogaudio Addr-Seq or similar)
CV OUT → VCO In Pitch
GATE OUT → ADSR In Gate, VCA In CV
```

### Chord/Unison with Poly Expander

```
(Main patching as above, plus...)
POLY CV OUT 1-7 (expander) → Multiple VCOs In Pitch
POLY GATE OUT 1-7 (expander) → VCAs In CV
(Mix all VCO outputs; mix all VCA outputs)
```

### Dynamic Expression with Accent

```
(Main patching as above, plus...)
ACCENT OUT → VCA In CV (for accent boost)
Mix: (VCA output, regular) + (VCA output, accented)
```

### Polyrhythmic Meditation Pattern

```
(Use Sands to set different strand lengths)
Rhythm: Length 3
Melody: Length 5
Legato: Length 7
(LCM = 105 steps before repetition)
Listen to the evolving pattern over ~6–7 minutes at 120 BPM
```

---

## Troubleshooting

### Pitch Too High

- Check **OCTAVE LO** and **OCTAVE HI** sliders
- If set to 0–2, the lowest notes will still be quite high (octaves 0–2)
- Decrease the sliders further or transpose down using the TRANSPOSE knob

### No Gate Output

- Ensure **MUTE** button is not lit (press to toggle)
- Check that **RUN GATE IN** is high (or not connected, which defaults to high)
- Verify **REST** knob is below ~0.5 (otherwise most steps are resting)
- Check clock is actually running (look at step LEDs)

### Poly Voices Not Sounding

- Ensure poly voice outputs are patched to VCOs and amplified
- Verify that poly voices' rest probability is low enough for them to play
- Check that mono is playing (if mono rests, all poly voices stay silent)

### Pattern Not Changing

- If **LOCK** button is lit, pattern changes are disabled (press LOCK to unlock)
- Try pressing **SCRAMBLE ALL** or **RESET ALL** buttons on Sands
- Verify **SEED IN** or DNA inputs aren't being held constant

### Accents Not Firing

- Check **ACCENT KNOB** is above 0 (probability > 0)
- Ensure **ACCENT OUT** is patched to a visible output (VCA, mixer, scope)
- Accent only fires on NewNote/Legato, not on Rest or Tie decisions
- Check that notes are actually sounding (gate must be > 5V for accent to register)

---

## Specifications

| Parameter | Range | Default |
|-----------|-------|---------|
| Octave Range | 0–8 | 2–5 |
| Transpose | −12 to +12 semitones | 0 |
| Note Length | 1/16, 1/16T, 1/8T, 1/8, 1/8D, 1/4T, 1/4, 1/2 | 1/8 |
| Rest Probability | 0–1 | 0.10 |
| Legato Probability | 0–1 | 0.10 |
| Accent Probability | 0–1 | 0.25 |
| Variation Amount | 0–1 | 0.50 |
| DNA Strand Lengths | 1–16 steps | 16 (all) |
| DNA Strand Offsets | 0–15 steps | 0 (all) |
| DNA Strand Rotations | 0–15 steps | 0 (all) |
| Poly Voices | 1–8 (1 mono + up to 7 poly) | 1 |
| Tempo | Clock-dependent | — |
| CPU Usage | ~2–3% (main + expanders) | — |

---

## Hardware Reference

MeloDicer for VCV Rack is inspired by the **Vermona meloDICER**, a hardware melodic sequencer with:
- 16-step pattern looping
- Independent rhythm, pitch, and expression controls
- Dice-roll randomization for each parameter
- Expander support for extended functionality

The VCV version builds on these concepts with:
- Continuous parameter windowing (DNA strands)
- Polyphonic voice expansion
- Full CV modulation support
- Accent gates for dynamic emphasis

---

## Version History

### v2.1.0 (Current)
- Added accent gates (mono and poly)
- Improved rest priority logic (respects fractional note tails)
- Poly voice state capture for smooth gate handoff
- Bug fixes: double-dot filename, DNA scramble/reset behavior

### v2.0.0
- Initial public release
- Full poly voice support (8 voices total)
- DNA expander with 6 independent strands
- Tick synchronization between mono and poly

### v1.0.0
- Mono sequencer with basic rhythm/pitch/octave control
- Simple rest and legato probabilities

---

## Credits & License

**Monsoon VCV Module** © 2026  
Inspired by Vermona meloDICER hardware sequencer.

For updates, issues, and feature requests, visit: https://github.com/Rodneyh303/Red-Dot-Modular

---

## Contact & Support

For questions, suggestions, or bug reports:
- GitHub Issues: https://github.com/Rodneyh303/Red-Dot-Modular/issues
- Email: rodneyh@bigpond.net.au

---

*This documentation reflects the current state of Monsoon as of v2.1.0. Features and specifications are subject to change.*
