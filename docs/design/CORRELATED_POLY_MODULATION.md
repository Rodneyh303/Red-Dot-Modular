# Correlated poly modulation — two taps, two axes, MPE as ONE consumer (concept)

**STATUS: FRAMING / CONCEPT NOTE.** Sits ABOVE three implementation docs and points down into them:
[[CA_EXPRESSION_CV_CORRELATION]] (the CA tap), [[INTERTROPICAL_SPEC]] (the arranged tap),
[[MPE_UTILITY_BUILD_SPEC]] (Keppel, one consumer). No new machinery — this is the idea that ties the
three together, recorded so it isn't lost. Related: [[MULTIGROUP_CONSERVATION_AND_CORRELATION]].

## The primitive
The thing being built is **correlated poly modulation**: the user patches their OWN per-voice modulation
(envelopes, LFOs, any poly CV) into a poly IN, and it comes back on a poly OUT with its voice channels
permuted by the SAME order/chaos state (CA's scatter/collapse/rotate/reflect) that reorders the note
material — so the modulation tracks the notes through the verbs. Each access is therefore a **poly-CV
in/out PAIR (16 channels in, 16 out) — a pass-through router, NOT an internally generated stream**
([[CA_EXPRESSION_CV_CORRELATION]] is the implementation spec). **MPE is one CONSUMER of this primitive,
not its definition.** The OUTPUTS are plain poly-CV; nothing about them is MPE-specific until patched into
Keppel.

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

## Count: distinct correlations vs parallel routing capacity (Rodney)
CORRECTION to an earlier framing here. The 3R/3M/2QM are NOT 8 generated "sources," and NOT 8 scalar
streams packed onto one poly cable. Per the primitive above and [[CA_EXPRESSION_CV_CORRELATION]], each is
a **polyphonic in/out PAIR** (16ch in, 16ch out): the user patches their own modulation in, CA returns it
with voice channels permuted by that stream's live correlation. Eight pairs = eight poly ins + eight poly
outs; they do NOT collapse onto one cable. (So an earlier "8 fit on one poly jack, channel = pair" idea is
wrong — that confuses the 16 VOICE channels inside one pair with the 8 pairs.)

Two quantities, attached to different structures:
- **Distinct correlations = 3, structural.** CA maintains one voice-permutation table per stream —
  rhythm (`rhythmSrc[16]`), melody (`melodySrc[16]`), q-mix (not built yet; 2 of the 3 exist in code
  today). The verbs mutate these tables. This count is small and set by how many order/chaos shuffles the
  engine keeps; it does NOT grow because you want more jacks.
- **Pairs per correlation = the 3/3/2 itself = parallel poly ROUTING CAPACITY.** The three rhythm pairs
  all share the ONE rhythm permutation; they differ only in WHICH user signal rides it (filter env on one,
  wavefolder on another, pan on a third). THIS is the demand-driven number, and it is what "3/3/2 might be
  limiting" actually means: a user wanting to rhythm-correlate a 4th independent poly modulation has run
  out of lanes. A 4th rhythm pair is NOT a new correlation or a "4th aspect of rhythm" — it is another
  parallel carrier of the SAME rhythm shuffle.

Fan-out vs a new pair (keeps capacity honest):
- Same OUTPUT signal to many destinations → **fan-out**: split one output poly cable. Free, no new pair.
- A DIFFERENT INPUT signal under the same correlation → **a new pair** (a new poly in/out lane). Not free
  — panel + plumbing.
Multiple Intertropicals give the analogous parallel capacity in PART space (each is its own poly router
over the arranged frame — [[INTERTROPICAL_SPEC]]).

Conclusion (the earlier one, now for the right reason): don't hardcode 3/3/2. The 3 distinct correlations
are structural and stay put; **parameterise the pairs-per-stream counts** so capacity is a build constant
that is trivial to rebalance — [[CA_EXPRESSION_CV_CORRELATION]] already frames it as "rebalancing is a
constant, not a redesign." q-mix's 2-vs-3 parity is the same capacity lever. Not urgent — noted so the
ceiling assumption doesn't calcify in the build.

## Boundary policy: unmapped outputs = 0V; the user owns the signal (Rodney)
Build-time policy for the (unbuilt) output side, CA pairs and Intertropical alike. Small by design: no
state, no parameter, no smoothing.

**Scope split — what the module owns vs what the user owns.**
- **Mapped output → reflect the input, exactly, including across boundaries.** Permute the channel
  ADDRESS, never touch the VALUE. Zeroing or slewing a mapped output would stop this being a
  pass-through router and start editing signals it doesn't own.
- **Unmapped output (slot with no member in the current scene) → 0V.** This is the ONE value the module
  is forced to invent — nothing was patched to produce it — so it cannot be delegated to the user.

**Why 0V, not last-held.** Last-held is better for the COMMON case (LFO/envelope: freezes, tail decays
naturally) but catastrophic for gates: a gate held high when its part leaves the scene NEVER releases —
a hung voice with no path to resolution from inside the patch. No single value is right for all content,
because the right answer depends on signal SEMANTICS and a poly router can't know them. So default to the
recoverable failure:
- Wrong on an LFO → a step. Audible, sometimes wanted, always fixable downstream (S&H or slew rebuilds
  last-held using the user's own modules, with their own timing).
- Wrong on a gate → a hang. Nothing downstream can clear it; there is no edge to trigger on.
0V is also the honest statement of the situation ("nothing is mapped here") rather than the module
asserting a value with no source behind it.
- **Check at build:** match whatever Intertropical's existing GATE/CV outputs already do at scene
  boundaries, so modulation and notes don't diverge into two conventions at the same instant.

**Switching instant.** Modulation must switch on the SAME boundary instant as the note routing, not a
block later — so a re-articulating voice's envelope masks the step (see below).

**Advice (guidance, NOT behaviour): rest your signals at phrase boundaries.** Terminate gates and bring
CV to/near zero at phrase boundaries and transitions are inherently clean, whatever the arranger does.
Costs the module nothing and can't be got wrong. Applies symmetrically to SUBSTITUTION (a voice handover
between scenes) as well as to leaving/arriving parts: both are smooth when outgoing and incoming signals
are at rest. Frame as "how to get glitch-free results", NOT "the correct way to patch" — routing a
free-running LFO through a scene switcher FOR the steps is using this correctly. Users may want glitches.

**Beat-matched material rarely exercises any of this.** When a change lands on a note boundary the step is
masked by the new note's envelope starting from zero — indistinguishable from the next note simply
sounding different. The exposed case is a destination SUSTAINING across the boundary (held pad, long
release tail, reverb send), where there is no envelope restart to hide behind.

**Not a dot.modular quirk (Rodney).** This discontinuity is a property of SWITCHING SIGNALS, not of
Intertropical: any Rack sequential switch, scene/preset changer, or router that reassigns CV or gate
mid-flight has exactly the same issue, gate-hang included. Document it as general Rack patching practice
that applies here, not as an apology for this module.
