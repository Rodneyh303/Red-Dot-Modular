# Colonnades (Micro-12) panel -- lift-and-shift from Monsoon note faders

Rodney's direction: the Micro-12 panel should read as a MONSOON FAMILY MEMBER, not a new visual
language. It is a lift-and-shift of the Monsoon note-fader block, widened, with the cents knobs
staggered below. Consistency with Monsoon is the goal.

## What to carry over from Monsoon (exact geometry)

### The 12 note faders -- SAME spacing as Monsoon
Monsoon note faders (embed_monsoon.py:6): `SEMI{i}_PARAM` at X = `7.5 + i*9.0`, Y = 59.75, for
i in 0..11. The **9.0mm horizontal pitch** is the consistency anchor -- Colonnades faders must use
the same 9.0mm pitch so the two panels visually rhyme. Do not invent a new spacing.

- 12 faders, 9.0mm pitch, same fader track/handle styling as Monsoon.
- Widen the panel to accommodate: 12 faders * 9.0mm + margins. Monsoon fits them in its lower block;
  Colonnades gives them the whole width, so it will be wider than CC's current 14HP. Size to the
  faders at 9.0mm pitch plus margins (~16-18HP likely; let the fader math set it, not a guessed HP).

### Numbered, NOT note-labelled (Rodney)
Monsoon labels its faders with note names (C, C#, D...). Colonnades faders are NUMBERED 1..12 like
the Monsoon STEP numbers (embed_monsoon.py has the `1 2 3 ... 12` strip under the faders), NOT
note-labelled -- because in an arbitrary tuning the degrees are not notes. Use the same numbering
style/position as Monsoon's step-number strip.

### Light sliders that light when the note is on (Rodney)
Monsoon uses `MonsoonLightSlider : VCVLightSlider<TLightBase>` (MonsoonWidget.cpp:33) -- a light
slider that reflects note-on state. Lift this pattern:
- Widget: a `ColonnadesLightSlider : VCVLightSlider<TLightBase>` mirroring MonsoonLightSlider.
- Binding: `bindLightParamsContiguous<...>` (SvgPanelKit.hpp:246 idiom) over `param_weight_0..11`.
- Lighting source: Monsoon lights the fader from note-on / scale-mask state
  (`m->modViz.pitchLane[sem]` at MonsoonWidget.cpp:74-75, and `semiOutOfScale` dims out-of-scale
  faders to 0.4 alpha at :57). Colonnades reads the SAME viz -- the Micro owns the weight[] mask now,
  so the light reflects which degree is active/sounding. Reuse the dim-out-of-scale idiom for
  disabled degrees (weight = 0 -> dimmed).

### Modulation arcs like Monsoon (Rodney)
Monsoon draws mod arcs via `ModArcOverlay.hpp` + `queueModArcLinear` / `flushModArcs`
(MonsoonWidget.cpp:146-169). The arcs show CV/Interchange modulation on top of each control. Lift
this for Colonnades:
- The Interchange expander modulates Colonnades faders (per MONSOON_MICRO_SPEC -- Interchange gains
  followTarget + targetHalf). So the mod arcs show Interchange modulation on the weight faders.
- Same queue/flush pattern: attach a ModArcOverlay to each fader after binding, wired to read the
  modulation amount. Copy the MonsoonWidget flushModArcs path.

## What is NEW (not lifted -- the cents knobs)

### Cents knobs staggered on two rows below the faders (Rodney)
Below the faders, the per-degree CENTS knobs. Rodney: "plenty of vertical range, stagger them on two
rows, alternating, below the faders." So:
- Even-index degrees (0,2,4,6,8,10) on one row, odd-index (1,3,5,7,9,11) on a row slightly below,
  alternating -- a zigzag. This gives each knob more horizontal room than a single tight row of 12
  would, and uses the vertical space Colonnades has.
- Each cents knob sits under its fader's X (so degree i's knob aligns with degree i's fader),
  just staggered in Y by even/odd.
- Root (degree 0) has NO cents knob -- locked at 0. CC's current panel already handles this with a
  "locked plate" (gen_monsoon_micro_12.py:79). Keep that; the root's slot in the stagger is the
  locked plate.
- Cents knob styling: same dot.modular knob family as Monsoon's Big-5 knobs (DMKnobs / the flat
  Befaco-style knobs), scaled down to fit 12 across in two staggered rows.

## What CC already has right (keep)
- The weight-fader-plus-cents-knob-per-degree concept (gen_monsoon_micro_12.py).
- Root cents locked, no knob, locked plate.
- The dark/light theme dicts, the dot.modular palette.
- `param_weight_<i>` and `param_cents_<i>` marker naming.
- ConnectMark (`light_connect`) for the claim indicator.

## What to change from CC's current attempt
1. WIDEN -- 12 faders at Monsoon's 9.0mm pitch, not squeezed into 14HP.
2. Faders NUMBERED 1..12, not the current layout -- match Monsoon's step-number strip style.
3. Faders become LIGHT SLIDERS (ColonnadesLightSlider), lit from the active-degree viz, dimmed when
   weight=0 -- lift MonsoonLightSlider.
4. Add MOD ARCS on the faders -- lift ModArcOverlay + queueModArcLinear/flushModArcs.
5. Cents knobs STAGGERED two-row zigzag below the faders, aligned in X to their faders.

## Cross-refs
- panel_src/embed_monsoon.py:6 -- the 9.0mm fader pitch to match.
- src/MonsoonWidget.cpp:33-90 -- MonsoonLightSlider (the light-slider + dim-out-of-scale to lift).
- src/MonsoonWidget.cpp:146-169 -- queueModArcLinear/flushModArcs (mod-arc pattern to lift).
- src/ui/ModArcOverlay.hpp -- the arc overlay widget.
- src/ui/SvgPanelKit.hpp:246 -- bindLightParamsContiguous idiom.
- MONSOON_MICRO_SPEC.md -- Micro-12 semantics (weight[] ownership, Interchange modulation).
- MONSOON_MICRO_CLAUDE_CODE_GUIDE.md -- the Micro build guide this panel serves.
