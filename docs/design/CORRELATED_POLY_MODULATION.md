# Correlated poly modulation — two taps, two axes, MPE as ONE consumer (concept)

**STATUS: FRAMING / CONCEPT NOTE.** Sits ABOVE three implementation docs and points down into them:
[[CA_EXPRESSION_CV_CORRELATION]] (the CA tap), [[INTERTROPICAL_SPEC]] (the arranged tap),
[[MPE_UTILITY_BUILD_SPEC]] (Keppel, one consumer). No new machinery — this is the idea that ties the
three together, recorded so it isn't lost. Related: [[MULTIGROUP_CONSERVATION_AND_CORRELATION]].

## The primitive
The thing being built is **correlated poly modulation**: per-voice CV whose channels are permuted by
the SAME order/chaos state (CA's scatter/collapse/rotate/reflect) that reorders the note material, so
modulation tracks the notes through the verbs. **MPE is one CONSUMER of this primitive, not its
definition.** The taps are plain poly-CV; nothing about them is MPE-specific until patched into Keppel.

## Two taps, two axes (the symmetry)
The instrument has TWO polyphony axes, and correlated modulation is available at BOTH — each tap at the
module that owns its axis:
- **Vertical / voice budget (Straits' 16 voices) → tap at CA, in VOICE space.** CA's 3R/3M/2QM
  correlated pairs ([[CA_EXPRESSION_CV_CORRELATION]]). The 16-voice frame.
- **Horizontal / part budget (the arranger's ≤8 parts) → tap at Intertropical, in PART space.**
  Intertropical carries CA's correlated CV through its voice→slot→output mapping, the SAME transform as
  the notes ([[INTERTROPICAL_SPEC]] "Routing through Intertropical"). The ≤8-part frame.

This is not two arbitrary jacks — it is the vertical→horizontal **conservation law** (16 voices
conserved down to ≤8 slots) surfacing in the MODULATION domain. The taps mirror the polyphony. That's
why it composes instead of bolting on.

## ONE correlation state, observed at two points (the invariant)
The two taps are **NOT two correlation engines.** There is exactly one CA per chain and it is the single
correlation source. Intertropical's tap is the **arranged VIEW** of CA's correlation — the same state
carried through the arranger's mapping. So "tap at CA" vs "tap at Intertropical" is not a choice between
two correlations; it is a choice of **which frame you read the one correlation in.**

**User rule (falls out clean): tap at the same stage your carrier comes from.**
- Voice-space synth (fed from Straits/voice outs) → read modulation at **CA**.
- Part-space / arranged rig (anything downstream of Intertropical) → read modulation at **Intertropical**.
Read the modulation in the frame your carrier (note + gate) lives in and it stays aligned by construction.
This is the same law the whole system runs on: ONE authoritative voice frame; every consumer reads it in
its own frame, never mixes frames.

## Keppel is frame-AGNOSTIC (Rodney)
Keppel is not wired to one tap. It takes CA's poly-CV **or** Intertropical's poly-CV **depending on where
Keppel's own gate + pitch source is**:
- Keppel's notes/gates from **Straits (voice space)** → feed Keppel the **CA** correlated CV.
- Keppel's notes/gates from **Intertropical (arranged part space)** → feed Keppel the **Intertropical**
  correlated CV.
Same Keppel, two rigs. The discipline is just the user rule above applied to Keppel itself: expression
source must share the frame of the note/gate source, or pitch and expression correlate to different
states. Match the tap to the carrier.

## MPE generalised beyond microtonality
Keppel began as "arbitrary tunings into a 12-TET MPE synth via per-note bend" — microtonality was the
whole pitch. It has since slipped that leash:
- X (bend) is also a patchable expressive-bend input (vibrato/scoops on plain 12-TET; the no-controller
  user). [[MPE_UTILITY_BUILD_SPEC]] "X input".
- Y/Z/velocity are correlated general modulation via these taps.
So **microtonality is now ONE dimension's ONE use case**, not the reason the module exists. Keppel's
identity is now **"generative correlated MPE expression source"**; microtonal-MPE-out is a corollary.
Stronger identity, because it's true for the 12-TET player who never opens a .scl file.

Because the taps are plain poly-CV, they modulate **anything** per-voice — filter cutoff, wavefolder,
pan, FX send — just as readily as CC74/pressure. Calling them "correlated modulation outs" rather than
"MPE outs" adds reach at ZERO cost (poly-CV is already universal). MPE is simply the consumer that
happens to interpret them as expression.

**Settles pass-through vs bundled (in favour of pass-through).** If these are general modulation they
MUST be plain poly-CV outs anything can eat, with Keppel one destination among many — not envelopes baked
into Keppel's private path. The earlier open question in [[MPE_UTILITY_BUILD_SPEC]] resolves here.

## Discipline (so the generalization stays free)
"General modulation" is FRAMING and REACH — a description of what the taps can drive — NOT a licence to
grow a modulation matrix. The taps remain exactly what they are: CA's 3R/3M/2QM correlated pairs and
Intertropical's arranged mirror of them. Reach, not new machinery. If a change here starts adding routing
surfaces or per-tap state, it has stopped being this idea.
