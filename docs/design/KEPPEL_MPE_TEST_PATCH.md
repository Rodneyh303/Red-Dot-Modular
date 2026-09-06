# Keppel MPE — reference test patch & validation tiers (PARKED)

**STATUS: PARKED SPEC — doubles as the Library demo patch. No code. After Keppel build-verify.**
Captured 2026-09-xx. Related: [[MPE_UTILITY_BUILD_SPEC]] (Keppel itself),
[[CA_EXPRESSION_CV_CORRELATION]] (the correlated expression that eventually feeds Y/Z).

## Why this matters
MPE OUTPUT is only as good as what receives it — receivers vary in what they honour (bend range,
CC74, poly pressure, member-channel count). So validation is TIERED, most-controllable →
most-real-world. The tier order mirrors the risk: isolate Keppel's correctness first, then add a
well-behaved receiver, then the messy real world.

## Tier 1 — Rack round-trip, NO external anything (deterministic, CI-friendly)
Keppel OUT → an MPE-IN path back into Rack → the reverse-calc monitor. This is the go/no-go already
specced in [[MPE_UTILITY_BUILD_SPEC]]. Validates that what Keppel SENDS is numerically correct
(<1–2 cents, right member channels, right CCs) BEFORE any receiver quirks enter. Fully deterministic,
the only truly CI-able test. **Do this FIRST** — it separates "Keppel is correct" from "the receiver
is configured right".

## Tier 2 — Surge XT (the RIGHT primary target + the reproducible demo)
Surge XT is the reference receiver, for four reasons that stack:
1. **Free + open-source** — anyone can replicate the exact patch; no DAW licensing.
2. **MPE-complete** — honours per-note bend, CC74 (timbre), channel pressure properly.
3. **Scala-aware** — loads .scl/.kbm tunings NATIVELY. This gives a killer cross-check (see below).
4. **Familiarity** — the VCV community already knows Surge (it also ships a large module set for
   Rack), so a Surge-based demo lands with people.

**CORRECTION (verified 2026-09-xx): use Surge as a PLUGIN/STANDALONE, NOT its Rack modules, for MPE.**
The Surge XT *VCV Rack* modules are ~36 CV/gate DSP building blocks (VCOs, filters, FX, LFO/EG) with a
4-input CV mod matrix — they are NOT a MIDI/MPE receiver. Patching Keppel → Surge Rack VCOs would feed
poly CV directly and NEVER exercise Keppel's MIDI/MPE output — it makes sound but validates NOTHING
about MPE (a false positive). To test MPE you need a real MPE MIDI receiver: route MIDI OUT of Rack
(loopMIDI / IAC / virtual port) into **Surge XT as a plugin or standalone**, which is fully MPE- and
Scala-capable. So Tier 2 requires MIDI-out plumbing; it is NOT a single zero-plumbing .vcv file.
(The Surge Rack modules remain a fine in-Rack SOUND source — just never mistake a poly-CV patch for
MPE validation.)

### The microtonal cross-check (Surge's Scala support = unique validation)
Keppel expresses arbitrary tunings into a 12-TET synth VIA per-note bend (note + member-channel bend
= the actual tuning). Surge can ALSO load the SAME .scl natively. So:
- Load tuning T into dot.modular (Sikit/Colonnades/Micro) → Keppel bends each note to T.
- Load the SAME .scl into Surge NATIVELY.
- Compare: Keppel-via-bend into a 12-TET Surge patch vs Surge-native-tuned. Pitches should match to
  within the round-trip bound (sub-cent per MpeMath). This validates microtonal-MPE-out against an
  independent implementation of the same tuning — a genuinely strong, independent check.

### Surge settings to match (pin at build time — verify against current Surge)
- Enable MPE in Surge.
- **Match the per-note bend range** to Keppel's (default 2; if using wide range 1..48, set Surge to
  the same). Bend-range mismatch is the #1 "sounds detuned/wrong" cause.
- Member/voice count consistent with Keppel's zone (15 members; VCV poly 16 → 15-member boundary,
  see MPE_UTILITY_BUILD_SPEC §"15-vs-16").
- For the microtonal cross-check: 12-TET Surge patch for the via-bend path; native .scl load for the
  reference path.

## Tier 3 — DAW (Bitwig) — real-world demo, LAST
Bitwig has genuinely good MPE and is Rodney's DAW → the impressive showcase (Rack GENERATING MPE into
Bitwig's expressive devices). But it's LAST: least controllable, least reproducible for other users,
needs MIDI routing out of Rack. Use for the showcase video, not for the reproducible/CI validation.

## What to route / listen / scope for (the actual patch)
- **Notes + per-note bend (X):** poly Monsoon → Keppel → Surge breakout. Scope MONITOR vs PITCH
  (should overlay). Play a chord where voices bend independently — confirm each voice bends alone
  (the "bend on only C" class of receiver bug = member channels collapsed; the Tier-2 Surge target
  avoids this since it's well-behaved).
- **Microtonal (X via bend):** the cross-check above.
- **Velocity (accent):** poly TB-303 accent — accented notes louder; confirm 2-level.
- **Y (CC74 / timbre) + Z (pressure):** once built — Monsoon/Intertropical poly gates → envelopes →
  (optionally CA correlated pairs) → Keppel Y/Z. Confirm accented notes get the accent LAYER B added
  (brighter/more pressure). Scope the reconstructed Y/Z from the extended monitor.
- **Re-articulation:** drive a slide past ±range; confirm correct pitch with retrigger (re-articulate
  on) and that Y/Z carry across the retrigger WITHOUT discontinuity.

## Deliverables
- **Reference demo:** Keppel → (MIDI out of Rack) → Surge XT plugin/standalone with a matching .scl —
  the MPE + microtonal-cross-check demo. Needs a virtual MIDI port, so document the routing. NOT a
  single self-contained .vcv (MPE must leave Rack). A Surge-Rack-modules patch can accompany it as an
  in-Rack SOUND demo, clearly labelled "not an MPE test".
- **Round-trip test (Tier 1):** the CI/correctness guarantee.
- **Bitwig showcase:** the video, not the repro artifact.

## Build order (mirrors risk)
Rack round-trip (deterministic, in-Rack, the only true in-Rack MPE test) → Surge XT plugin/standalone
via MIDI-out (open, MPE-complete, microtonal cross-check) → Bitwig (real-world showcase). Keep
round-trip as the CI guarantee; use Surge for the reproducible microtonal validation; Bitwig for video.
