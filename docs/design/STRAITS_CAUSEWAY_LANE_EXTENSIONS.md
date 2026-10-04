# Straits / Causeway — per-lane panel-extension model (Rodney)

STATUS: DESIGN DIRECTION. Post-consolidation (hard prerequisites below). The natural next
generalisation after lane uniformity: once every RHYTHM lane is the same kind of thing (poly +
correlated, no mono special cases), a panel should not hard-code WHICH lanes it exposes.

## The problem it solves
Straits grew lane-by-lane (rest, accent, then qmix), each lane = **16 per-voice knobs**. Adding
variation + legato = **5 lane-sets x 16 = 80 knobs**. Hard-coding that is bad either way: cram all 5
(huge, mostly unused) or pick a subset (= re-introducing "which lanes are standard" special-casing).
Causeway's modulation INPUTS have the same shape. Users want only the lanes they use.

## Model: TRUE PANEL EXTENSION (option B, Rodney's choice)
NOT a context-menu show/hide on a fixed panel. Each rhythm lane is a physically attachable EXTENSION
that docks on and GROWS the panel by that lane's width (16 knobs on Straits; 16 CV ins on Causeway).
Pay HP only for the lanes you use.
- **Scope = the RHYTHM FAMILY only:** REST, ACCENT, QMIX, VARIATION, LEGATO. **Octave is OUT** (it is
  pitch, not rhythm).
- **Base frame = frame + QMIX** (shown by default — qmix is the defining sequencer-quantiser axis after
  the mode collapse, so its primacy is structural). A bare Straits is "per-voice qmix".
- **Four attachable extensions:** REST, ACCENT, VARIATION, LEGATO. Add what you want.
- **SEAMLESS rendering (TwoWayExpander-style):** base + lane expanders must render as ONE continuous
  panel — the expander's panel art abuts the base's exactly (shared background, rails, and the
  per-voice knob-row Y aligned across the seam), so base + N expanders look like a single wider
  module, not separate modules with a visible gap. Reference: VGLabs TwoWayExpander
  (github.com/landgrvi/VGLabs-TwoWayExpander). This is a MODEL property, not just cosmetic — the
  lane-per-voice knob rows must flow unbroken across each boundary.
- Applies to **both** Straits (per-voice knobs) and Causeway (per-voice CV mod inputs) — same model,
  two roles.

## FIXED-ORDER, RIGHT-SIDE, CONTIGUOUS docking (Rodney — the simplification)
Not flexible connection — the expanders dock ONLY to the RIGHT, in a FIXED CANONICAL ORDER, read
CONTIGUOUSLY from the base. This sidesteps nearly all expander complexity:
- **Position encodes identity.** A fixed order (e.g. REST -> ACCENT -> VARIATION -> LEGATO) means the
  base knows what the Nth right-neighbour IS by position — no discovery/identification of "which lane
  is this expander".
- **Plain Rack expander chaining** (leftExpander/rightExpander), NOT the CA flexible-connection model.
  So this likely does NOT need the connection rework as a prerequisite (that still stands for CA marks).
- **Gap rule:** lanes are read contiguously rightward until the first non-matching/absent neighbour;
  anything past a gap is ignored. User learns "keep them contiguous, in order". Simple and predictable.
- **Order rule:** pick one — base assigns lanes by POSITION regardless of intent, OR base only
  recognises the expected expander at each position. Decide at build; position-assigns is simplest.
- Deterministic rightward growth; no scattered-expander confusion.

## Prerequisites (loosened by fixed-order docking)
1. **Lane uniformity must be REAL in the engine (STILL REQUIRED).** VARIATION and LEGATO have no poly
   buffers / no spread yet (SANDS_CONSOLIDATION.md "spread on VAR/LEG"). A variation/legato extension
   cannot carry correlated per-voice control until the Sands consolidation builds those lanes. So:
   **Sands consolidation first.**
2. **Connection rework — NO LONGER a prerequisite for THIS** (fixed-order right docking uses plain Rack
   expander chaining, not the CA connection model). It remains needed for CA marks, but it no longer
   BLOCKS lane extensions, so it can run in PARALLEL rather than before.

## Implementation: POINTER-based (zero delay)
Base and lane expanders communicate via direct POINTERS (as the existing MonsoonExpanderManager /
MonsoonDiscovery pattern does), NOT Rack's 1-block expander message passing — so lane values are
read with ZERO sample delay. The base resolves its contiguous right-neighbour chain to pointers once
per block (on expander topology change), then reads lane state directly. No per-sample message copy,
no 1-block latency. Fixed-order + position-encodes-identity makes the pointer resolution trivial: walk
rightExpander, assign by position.

## Build steps
1. Base Straits = frame + QMIX (default). Move REST+ACCENT out of base into lane expanders.
2. Seamless expander scaffold: one lane-expander type, docks right fixed-order, panel art abuts the
   base seamlessly, base resolves the right-chain to pointers. Prove with ONE expander (REST).
3. Port REST, ACCENT onto the scaffold.
4. Add VARIATION, LEGATO lane expanders.
5. Registration + plugin.json + seamless panel generators.
Open: final lane ORDER; ONE "Straits Lane" slug (position=lane) vs separate slugs per lane; Straits
stays rhythm-family only (pitch in Sands). Same model applies to CAUSEWAY (CV mod inputs instead of
per-voice knobs).

## Dependency chain / order (shortened)
mode collapse (6->3) -> Sands consolidation (lanes poly+correlated, uniform) -> THEN Straits/Causeway
lane extensions. (Connection rework parallel, not blocking this.)

## Possible further unification (note, not committed)
Straits (per-voice knobs), Causeway (per-voice CV), and the unified Sands (visual editor) may all be
the SAME configurable-lane frame in different roles — lane uniformity subsuming three modules' worth of
special-casing, as it already did for the three Sands. Worth revisiting once the extension model exists.


## REFINED MODEL (Rodney, supersedes base=frame+QMIX) — five refinements
These sharpen (and in two cases CHANGE) the model above. Where they conflict, THESE win.

1. **Base = minimal frame + ALL IO jacks + docking. No lane controls in the base.** The base carries
   every input/output (the complete signal interface) and the docking frame — nothing lane-specific.
   So the base alone is fully functional; expanders add CONTROL RESOLUTION, not capability, and never
   route signal across the seam (just pointer-shared control state). (Changes the doc's per-lane-IO.)

2. **QMIX is an EXTENDER, not baked into the base** — just PRE-ADDED by default. So EVERY lane
   (QMIX included) is a lane-expander; zero special cases. A bare base has no lanes; QMIX is the
   default-attached one and can be removed like any other. (Changes "base = frame + QMIX".)

3. **ONE generic `LaneExpander` class, parameterised by a LANE DESCRIPTOR** (lane id, colour, label,
   engine strand/params to bind). REST/ACCENT/QMIX/VARIATION/LEGATO are each just a descriptor — NOT
   separate module classes. A docking/render bug fixed once fixes all lanes; adding a lane later is a
   new descriptor. This is lane-uniformity in the PANEL code (avoids the per-module divergence that
   made Sands a nightmare).

4. **No expander for a lane ⇒ that lane uses the MONO value (Monsoon's mono control), broadcast to all
   voices.** The expander is purely a per-voice OVERRIDE on top of the mono default that already
   exists. Layering: Monsoon = mono controls (the floor; all voices follow mono); each added
   lane-expander = per-voice control for THAT lane; absent expander = mono-broadcast. No lane is ever
   lost by lacking its expander — it just runs at mono (N=1) resolution. "Mono controls on Monsoon,
   mono+poly on Straits." Engine read: per-voice WHEN the expander is present AND the voice is
   independent; else MONO. (Note: "delegated to mono" and "no expander" are the same read: mono value.)

5. **Keep the OLD Straits registered and working throughout the build** — build the new base+expanders
   as SEPARATE new slugs alongside it (reference + safety net). Retire the old Straits slug LAST, only
   once the new system fully replaces it and is tested.

6. **Eventual context-menu add/remove lanes** on the base (dock/undock a lane-expander), QMIX
   pre-checked. Design the LaneExpander so it CAN be added that way (not hard-wired to browser
   placement) — but the docking mechanism exists first; the menu is a later UX layer.

## Build sequence (keeps old Straits, modular, incrementally testable)
- **Phase 0 — recon, write nothing:** current Straits (lanes/HP/panel-gen/how it reads per-voice);
  the MonsoonExpanderManager/MonsoonDiscovery pointer-neighbour infra; the seamless-abutment mechanics
  (shared bg/rails, per-voice knob-row Y aligned across the seam). Report the minimal one-lane scaffold.
- **Phase 1 — generic LaneExpander + base, proven with ONE lane (QMIX):** new Straits BASE (frame +
  all IO + docking); generic LaneExpander(descriptor); instantiate for QMIX; dock right, seamless
  render, pointer-wired. Test in Rack: base+QMIX = one seamless panel, per-voice QMIX works, old
  Straits still there. STOP and review before scaling.
- **Phase 2 — drop in REST/ACCENT/VARIATION/LEGATO as DESCRIPTORS** to the same class. Verify
  mono-default-when-absent for each (remove the expander → lane falls back to mono-broadcast).
- **Phase 3 — context-menu add/remove** (optional/later), QMIX default.
- **Phase 4 — retire old Straits** last, once fully replaced and tested.
Additive throughout (new slugs; don't touch working Sands/engine). Suite green between phases.
