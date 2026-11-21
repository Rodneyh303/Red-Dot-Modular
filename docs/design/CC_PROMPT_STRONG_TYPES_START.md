# CC PROMPT — STRONG TYPES (VoiceSlot/LaneId): make wrong indexing UN-COMPILABLE (seam eradication)

Implements SEAM_ERADICATION_PLAN.md Stage 2 (strong types, moved early). GOAL: end the index-convention
bug class (voice-0-special, polyBank-vs-voiceSlot, lane-drop) by making a WRONG index a COMPILE ERROR,
not a Rack-later bug. Same "one authority, structure-enforced" discipline as the resolver (values) +
rate manager (timing), now for ADDRESSING. C++20 confirmed green (CI). Branch off lane-expander-refactor.

## CURRENT STATE (verified — build on what exists, don't redo)
ALREADY DONE: the uniformity harness (test_lane_voice_uniformity.cpp + test_eradication_harness.cpp —
Stage 0, the GATE for this work); VoiceResolver (VoiceResolver.hpp) ALREADY holds the convention as
documented helpers: "ONE way code indexes per-voice storage: voiceSlot — 16-wide, 0=V1(mono)…15=V16"
(voiceSlot(v)=v-kMonoVoice, polyVoiceSlot, kMonoVoice, kVoiceSlots=16). No l<4/l<5 lane-truncation loops
remain. The resolver merge (PR #290) cleaned the macroOwnLaneProbability / warpRow(0)-in-prob-out residue.
WHAT REMAINS (the debt the types kill): ~83 BARE-INT per-voice/lane index sites in SequencerEngine.cpp —
chiefly voices[0].xxx and warpRow(0, PL_*) in executeStep (the MONO path, :552-768). These are the
LEGITIMATE mono ROLE (V1 is the anchor voice) but expressed as a bare-int [0] HARDCODE — indistinguishable
from residue, and nothing stops a wrong [0] elsewhere. The types make the mono index EXPLICIT + TYPED.

## THE TYPES (mint-only-via-VoiceResolver — the enforcement)
```
// VoiceSlot — the 16-wide storage index, 0=V1(mono)…15=V16. Mintable ONLY via VoiceResolver.
struct VoiceSlot {
    int v;
    explicit constexpr VoiceSlot(int raw) ... // keep construction INTERNAL to VoiceResolver;
                                              // NO implicit int→VoiceSlot (that's the whole point)
    // consumers get a VoiceSlot from VoiceResolver::mono() / ::slot(voiceNumber) / loop iterator —
    // NOT by writing VoiceSlot(0) at a call site.
};
struct LaneId {  int l;  /* 0..6 PL_* order; one canonical lane descriptor */  };
```
- VoiceResolver mints them: `VoiceResolver::mono()` → VoiceSlot{0} (V1); `VoiceResolver::slot(V)` →
  VoiceSlot{V-1}; a range/iterator over all 16. So `warpRow(0, …)` STOPS COMPILING — you must write
  `warpRow(VoiceResolver::mono(), LaneId{PL_MELODY})`, which is EXPLICIT + SEARCHABLE (grep mono()),
  not an anonymous magic 0.
- LaneId covers all 7; a lane loop iterates LaneId so it CAN'T stop at 4 and silently drop lane 6.
- The MONO ROLE is PRESERVED: executeStep legitimately uses VoiceResolver::mono() — that's correct and
  INTENDED (V1 is the anchor; poly follows mono's MonoDecision). The type makes "this is deliberately
  the mono voice" explicit, vs "someone hardcoded 0". Do NOT try to remove the mono role — type it.

## THE WORK (incremental — convert, keep harness green each step)
1. Define VoiceSlot + LaneId; add VoiceResolver::mono()/slot()/iterator minting (keep the existing
   int helpers working during migration — add typed overloads first, don't break callers).
2. Convert the hot per-voice/lane accessors to take VoiceSlot/LaneId: warpRow, polyRandom/polyRandomSrc,
   the per-voice banks, voices[] indexing. Start with warpRow + voices[] (the 83 sites' core).
3. Convert call sites: voices[0] → voices[VoiceResolver::mono()], warpRow(0,PL_X) →
   warpRow(VoiceResolver::mono(), LaneId{PL_X}), poly loops → iterate VoiceResolver's range yielding
   VoiceSlot. The DEBT sites surface as compile errors as you tighten each accessor — that's the
   migration DOING the audit.
4. Remove the implicit int path once converted (make VoiceSlot construction VoiceResolver-only) so a
   bare int CANNOT index per-voice storage anymore. Grep-confirm: no raw `warpRow(0`, no `voices[0]`
   outside VoiceResolver::mono(), no bare-int per-voice index in the hot path.
INCREMENTAL: each accessor+its callsites in one commit, harness GREEN after each. Do NOT big-bang 83 sites.

## THE GATE (already exists — USE IT, don't reinvent)
test_lane_voice_uniformity.cpp asserts 16 voiceSlots × 7 lanes uniform via the real write path. It MUST
stay green through every conversion (the types are a no-op on behaviour — same indices, typed). If a
conversion changes behaviour, it's wrong. ADD one assertion: a VoiceResolver-minting round-trip
(mono()→VoiceSlot{0}, slot(V)→{V-1}, iterator covers 0..15 exactly once) so the minting is pinned.

## DONE =
- VoiceSlot + LaneId types, mint-only-via-VoiceResolver (no implicit int→VoiceSlot).
- warpRow / per-voice banks / voices[] take the types; call sites use VoiceResolver::mono()/slot()/iter.
- Grep-confirm: 0 raw `warpRow(0`, 0 bare `voices[0]` (all via mono()), 0 bare-int per-voice index in the
  hot path. A wrong index / magic-0 / polyBank confusion is now a COMPILE ERROR.
- Harness green throughout (behaviour unchanged); minting round-trip test added.
- bash test/run_all.sh green; plugin builds (C++20). CPU unchanged (types are compile-time).
- PR: "strong types — wrong per-voice/lane index now un-compilable; mono ROLE preserved as
  VoiceResolver::mono(), typed not hardcoded."

## NOT THIS PROMPT
- LaneId unifying the ui-lane-name system if separate (note it; a follow-up if it's a bigger split).
- The rate manager (next, builds ON these types) / the value-store audit (Stage 5).
- The mono ROLE itself (V1-as-anchor is correct — TYPE it, don't remove it).
