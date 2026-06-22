# Voice Resolver — the design for "Idea C" (revised)

**Status:** design spec. No engine code. Supersedes the *generation-unification* framing in
UNIFIED_VOICE0_SPEC.md (kept for history) with a **resolver** framing that matches the real
data flow and preserves mono's standalone nature.

---

## 1. The fundamental (what the resolver must honour)

The mono voice is **two things at once**, by design:

1. **The core voice from Monsoon** — the original standalone instrument. It plays correctly
   with nothing attached. Its generation must NOT depend on the polyphonic system existing.
2. **Voice 1 of 16 in the poly ensemble** — once East / Macro / poly attach, mono also
   participates: it's a tab, channel 1 of the poly out, and can be *observed and modulated*
   by the poly-aware modules.

Poly is **not copies of Monsoon**. Poly is a *parameterised re-derivation* of mono: it reads
specific mono values and re-rolls selectively (details in §3). So the relationship is
**directional** — mono is upstream; poly derives from it.

This kills the original "C-unify" idea (mono = voice 0 of one generation array): that routes
mono's generation through poly code, violating (1). The correct shape is a **resolver layer**:
mono generates standalone; the resolver lets poly-aware code *address all 16 voices uniformly*,
resolving voice 1 to mono and voices 2-16 to the poly derivation. **Unify the interface, not
the generation.**

---

## 2. The three-stage resolution shape (uniform across all 16 voices)

```
voice_value(V, lane, param) = modulate( derive( mono_source(lane, param), V ), V )
```

- **mono_source** — mono's own standalone draw (the Monsoon/Sands path). Untouched by the
  resolver; only read.
- **derive(·, V)** — how poly voice V re-derives from mono (per §3). For V=1 (mono) this is
  the identity.
- **modulate(·, V)** — the East-CV / Macro-global mix-in overlay (today's combineLOR/interp-Y
  blend). For V=1 it's mono's own CV only.

All 16 voices share this shape; only the *content* of derive/modulate differs, and for V=1
both collapse to mono's standalone path. That collapse is the whole point: voice 1 stops being
a special case in the surrounding code and becomes "the voice whose derive=identity and whose
base is owned (read-only) by mono."

---

## 3. THE DERIVATION TABLE (extracted from the live engine)

Reverse-engineered from `SequencerEngine::executePolyVoice` (src/dsp/engines/SequencerEngine.cpp
~L407), the `PolyVoice` contract comment (SequencerEngine.hpp ~L31), and the LOR/spread
combination in `MonsoonExpanderManager` (combineLOR/combineSpread). As predicted, it needs only
**two entries**: voice 1 (mono), and voices 2-16 (poly) — every poly voice follows the same rule.

### Entry A — Voice 1 (mono master strand)

| aspect | rule |
|---|---|
| generation | mono's own standalone path (6 lanes: REST/MEL/OCT/LEG/ACC/VAR), via MONO_LANE_TO_STRAND |
| derive | identity (it IS the source) |
| base ownership | mono (read-only to East/Macro — they mirror/display, never write) |
| modulate | mono's own CV (applyMonoCV / applyMonoSprCV) + (interp. Y) optional East/Macro mix-in overlay |
| spread mode | MONO_DRAW (self-target → no-op, the fixed anchor; cf. the spread self-target fix) |
| gate / timing | mono's own GateState (`gs`) — the master clock everything else follows |

### Entry B — Voices 2-16 (poly), each derived from mono

What poly **shares from mono** vs **owns / re-rolls** — the heart of the derivation:

| element | source | rule |
|---|---|---|
| **gate start / triggering** | mono | poly fires only when mono gate starts (NewNote, or Legato from dead). Poly never triggers independently — it *follows mono's rhythm gating*. (`monoGateStart`) |
| **note-length index (nvIdx)** | **mono** (shared) | poly uses `lastStepResult.nvIdx` — mono's note length for the step. Not re-drawn. |
| **legato / tie / sustain behaviour** | **mono** (shared) | poly mirrors mono's decision (Tie→extendHold, Legato→slide, MidNote→sustain). "Poly follows mono gate presence strictly if already in." |
| **scale / pitch quantisation** | **mono** (shared) | `genPitchLive` uses the shared scale/input. |
| **REST (rhythm) probability** | **poly owns** | each `PolyVoice` owns `restProb`; poly draws `polyRhythmRandom[v]` and rests if `r_rest < restProb`. **This is poly rhythm's divergence: vary on rest.** |
| **MELODY draw** | mono note + **poly probability** | poly melody uses the SAME note/octave faders as mono but its OWN `polyMelodyRandom[v]` (different probability vector). **Poly melody's divergence: same notes, different probability.** |
| **OCTAVE draw** | shared faders + **poly probability** | poly octave uses mono's octave faders with its own `polyOctaveRandom[v]`. |
| **per-lane LOR (len/off/rot)** | **poly owns** | `polyLen/Off/Rot[v][lane]` — East sets per voice/lane; Macro sets same lane for all voices. Independent index into the 16-step vectors. |
| **per-lane base value (L/O/R/spread)** | East-local OR Macro-global | the owner/opt-in latch picks; modulate adds the Macro-CV blend (send × macroCVDelta). |
| **spread mode** | poly | AVERAGE_POLY (ensemble-average target) — already a SpreadInterp::Target parameter, NOT a separate algorithm (confirmed §5a of the old spec). |

**Key insight for "future changes easier":** the divergences are few and *declared* —
rhythm re-rolls REST; melody/octave keep mono's note/octave but re-roll probability; everything
about *triggering, note-length, legato, scale* is inherited from mono. Today this is implicit,
spread across executePolyVoice's control flow. The resolver makes it a **two-entry table**:
changing "what poly shares vs re-rolls" becomes editing Entry B, not tracing gate logic.

---

## 4. What the resolver is (concretely)

A read/address layer the poly-aware modules (East, Macro, the UI tabs) call instead of indexing
banks directly:

```
resolver.value(V, lane, param)     // V in 1..16; V=1 → mono, V≥2 → poly derivation
resolver.baseEditableHere(V, lane) // false for V=1 (mono owns) → UI shows read-only mirror
resolver.polyBankIndex(V)          // V-2 for poly; undefined for mono (callers must not index)
resolver.spreadMode(V)             // MONO_DRAW for V=1, AVERAGE_POLY for V≥2
```

It does NOT generate. It reads `mono_source` (mono's standalone output) and the poly banks,
applies the Entry-B derivation/modulation, and presents one uniform 16-voice interface.

---

## 5. What dissolves (now evidence-backed, not theoretical)

Each item below is a real hack/tax from shipped commits that exists ONLY because there is no
resolver — the surrounding code does ad-hoc mono-vs-poly bridging:

- **The `-1` poly remap** (fix/voice1-tab-16): ~20 sites doing `selectedVoice-1` / `>=1` gating
  because tab index 0 = mono but poly banks are 0-based. Under the resolver, callers pass V
  (1..16) and the resolver maps internally. The remap vanishes.
- **V1-page cover-paints** (fix/voice1-tab-16): masking baked attenuverter/BASE-band art on V1
  because "mono isn't really a voice here." Under the resolver, V1 is "base not editable here" —
  the UI asks `baseEditableHere(1,·)` and lays out accordingly; no masking hacks.
- **Tab-1 mono-mirror special-cases** (tab1MonoMirror / readOnly): become `baseEditableHere`
  returning false for V=1. One predicate, not scattered branches.
- **Interp-Y manual mix-in** in readStrand + the mono spread loop: becomes the resolver's
  `modulate` stage, applied uniformly — no bespoke mono-blend code.
- **The one-frame macroCVDelta lag**: the resolver defines a single read order
  (mono_source → derive → modulate), removing the cross-manager sequencing that caused it.

---

## 6. Risk profile (better than C-unify, still needs care)

- **Mono generation is provably untouched** — the resolver only *reads* mono_source. So the
  standalone instrument can't regress. This halves the bit-exact validation surface vs C-unify.
- **The poly side still needs the parallel-run harness**: the resolver must reproduce
  executePolyVoice's derivation EXACTLY. Moving that logic from imperative control-flow into a
  declared table is the risky part — same notes must come out. No Rack SDK here → still a
  build-capable, harness-first session for the actual implementation.
- **Staging**: (1) build the resolver as a READ-ONLY shadow that callers can query, asserting
  its output == the current direct-bank/mono reads, bit-exact, across all voices & modes;
  (2) migrate callers (UI, prob-outs, modulation) to the resolver one at a time, each verified;
  (3) only once everything reads through the resolver, collapse the `-1`/cover/mirror hacks;
  (4) the generation paths (mono standalone, executePolyVoice) stay — the resolver wraps them.

Note: unlike C-unify, the resolver does NOT require touching executePolyVoice's note output at
all in the first instance — it can wrap the *existing* poly generation. The derivation TABLE is
documentation + the resolver's read logic; rewriting executePolyVoice to BE table-driven is a
later, optional step once the resolver is trusted.

---

## 7. Recommendation

The resolver is the right C: it honours mono-stands-alone (generation untouched), serves
mono-is-also-voice-1 (uniform 16-voice interface), and makes "what poly shares vs re-rolls" an
editable two-entry table instead of buried gate logic — which is the "future changes easier"
goal. It is meaningfully lower-risk than the generation-unification C because mono's note path
is never rewritten and the poly path can be wrapped before (if ever) being made table-driven.

Still a build-capable, harness-first project — but a smaller, safer one than the original spec
implied, and now grounded in the actual derivation table rather than an aspiration.
