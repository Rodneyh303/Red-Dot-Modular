#pragma once
// ── Voice Resolver ──────────────────────────────────────────────────────────────
// A uniform 16-voice ADDRESSING layer over the engine, per docs/design/
// VOICE_RESOLVER_SPEC.md. Voice 1 = the standalone mono master strand; voices 2..16 =
// the 15 poly voices (parameterised re-derivations of mono).
//
// STEP 1 — READ-ONLY SHADOW. This layer GENERATES NOTHING and CHANGES NO BEHAVIOUR. It
// is a thin read facade over accessors that already exist on SequencerEngine:
//   voice 1  → masterLaneProbability / masterLaneStep   (mono)
//   voice ≥2 → polyLaneProbability   / polyLaneStep      (poly, voiceSlot = voice-1)
// It exists so callers (UI tabs, prob-outs, modulation) can eventually address all 16
// voices through ONE interface instead of the scattered mono-vs-poly bridging (the -1
// remap, the tab-1 mirror special-cases, the cover-paints). Until callers are migrated
// (later, verified steps), nothing reads through this — so adding it is behaviour-inert
// and a build with it present must be bit-identical to one without.
//
// Voice numbering (matches the UI tabs V1..V16 and the channel-1-reserved convention):
//   V = 1        → mono master strand (the core Monsoon voice; standalone)
//   V = 2..16    → poly voice, voiceSlot = V - 1  (1..15)
//
// The resolver only READS. Mono's generation (the Monsoon/Sands path) and poly's
// generation (executePolyVoice) are untouched; this wraps their outputs.

#include "engines/SequencerEngine.hpp"

namespace dotModular {

struct VoiceResolver {
    const SequencerEngine& eng;
    explicit VoiceResolver(const SequencerEngine& e) : eng(e) {}

    // ── Voice identity ────────────────────────────────────────────────────────
    static constexpr int kMonoVoice  = 1;            // V1
    static constexpr int kFirstPoly  = 2;            // V2
    static constexpr int kLastVoice  = 16;           // V16 (mono + 15 poly)

    static constexpr bool isMono(int v)  { return v == kMonoVoice; }
    static constexpr bool isPoly(int v)  { return v >= kFirstPoly && v <= kLastVoice; }
    // Poly voiceSlot (1..15) for V2..V16 — the SAME value voiceSlot() returns for poly
    // voices (NOT a 0-based poly bank; that convention is dead since the COLLAPSE).
    // V1 (mono) returns -1. Callers must gate on isPoly().
    static constexpr int  polyVoiceSlot(int v) { return isPoly(v) ? (v - 1) : -1; }

    // ── Per-voice STORAGE SLOT convention (ONE, post-COLLAPSE) ─────────────────
    // There is ONE way code indexes per-voice storage: voiceSlot — 16-wide, 0 = V1 (mono),
    // 1..15 = V2..V16. The old 0-based poly bank (bank 0 = V2, i.e. v-2) is DEAD: the arrays
    // it used to index were widened to [16] with V1 at 0. The historical off-by-ones that bit
    // the Macro send/atten banks came from those two conventions coexisting.
    //
    //   voiceSlot(v)     — 16-wide, 0=V1 … 15=V16. THE storage convention.
    //   polyVoiceSlot(v) — the SAME value as voiceSlot for poly voices (V2→1 … V16→15),
    //                      -1 for V1. A poly-guard on the one convention, NOT a second
    //                      convention.
    //
    // The tie (asserted below): for any poly voice, voiceSlot(v) == polyVoiceSlot(v).
    // If you ever want a 0-based poly bank again, the ARRAY is wrong — fix the array.
    static constexpr int  kVoiceSlots = kLastVoice;          // 16 (mono + 15 poly)
    static constexpr int  voiceSlot(int v)  { return v - kMonoVoice; }   // V1→0 … V16→15
    static constexpr int  slotToVoice(int s){ return s + kMonoVoice; }   // inverse
    static constexpr int  kMonoSlot = 0;     // the mono slice: voiceSlot(kMonoVoice). Banks
                                             // with a mono mix-in read here; the off-by-one
                                             // was a poly voice wrongly landing on this slot.


    // Is the per-lane BASE editable on a poly-aware module (East/Macro) for this voice?
    // FALSE for mono: voice 1's base belongs to the standalone Mono module — East/Macro
    // mirror it read-only. (This is what tab1MonoMirror / the V1 cover-paints encode
    // today; the resolver makes it one predicate.)
    static constexpr bool baseEditableHere(int v) { return isPoly(v); }

    // Spread interpolation target for this voice: mono self-targets (fixed anchor),
    // poly converges toward the ensemble average. Already a SpreadInterp parameter, not
    // a separate algorithm — surfaced here as per-voice metadata.

    // ── Uniform per-lane reads (the point of the layer) ─────────────────────────
    // lane: SequencerEngine::PL_REST / PL_MELODY / PL_OCTAVE / PL_ACCENT (0/1/2/3).
    // Dispatches V1 → master strand, V2..V16 → the poly bank. Bit-identical to what
    // callers compute today by hand; this just removes the per-call branching.

    // Resolved probability draw (0..1) for (voice, lane) at the voice's own LOR step.
    float laneProbability(int v, int lane) const {
        if (isMono(v)) return eng.masterLaneProbability(lane);
        if (isPoly(v)) return eng.polyLaneProbability(lane, polyVoiceSlot(v));
        return 0.f;
    }

    // Resolved step index (for S&H edge detection) for (voice, lane).
    int laneStep(int v, int lane) const {
        if (isMono(v)) return eng.masterLaneStep(lane);
        if (isPoly(v)) return eng.polyLaneStep(lane, polyVoiceSlot(v));
        return -1;
    }

    // Probability sampled at an EXPLICIT step — Macro's prob-OUT source (the module
    // samples every voice at Macro's own global LOR step). CC_PROMPT_UI_TO_RESOLVER_SPEC
    // step 2: this is the ONE accessor (eng.effectiveProbability, MacroOwn lens — the
    // same call Macro's BARS make for the displayed voice), NEVER the shared final
    // (masterLaneProbability/finalRandomByStrand → warpRow → random_), which East's
    // spread writes on an East-owned lane (the backwards-delegation leak: East's spread
    // moved Macro's scope but not its bars). One-way borrowing: Macro never borrows East
    // — the MacroOwn lens reads macroOwnSpread only, so East's edit cannot reach the out.
    // v is a VOICE NUMBER (1..16) → the accessor's explicit voiceSlot; V1 → slot 0.
    // V1 uses the explicit step too (the old mono branch self-resolved the engine's mono
    // LOR step — Macro's own step is the out's sampling grid, same as the poly channels).
    float laneProbabilityAtStep(int v, int lane, int step) const {
        if (isMono(v)) return eng.effectiveProbability(/*voice=*/kMonoSlot, lane, step,
                                                       SequencerEngine::ProbLens::MacroOwn);
        if (isPoly(v)) return eng.effectiveProbability(polyVoiceSlot(v), lane, step,
                                                       SequencerEngine::ProbLens::MacroOwn);
        return 0.f;
    }

    // How many voices are currently live: mono (always) + the active poly count. Matches
    // the UI's setActiveCount(numPolyVoices + 1).
    int activeVoiceCount() const { return eng.numPolyVoices + 1; }

    // ── Compile-time self-checks on the voice numbering / index math ────────────
    // (Cheap insurance that the V→bank mapping can't silently drift.)
    static_assert(kMonoVoice == 1,  "mono is voice 1 (channel-1-reserved convention)");
    static_assert(kFirstPoly == 2,  "poly voices start at V2");
    static_assert(kLastVoice  == 16, "16 voices total: mono + 15 poly");
    static_assert(kLastVoice - kFirstPoly + 1 == 15, "exactly 15 poly bank slots");
};

// Free, header-scope checks of the index dispatch (no engine needed).
inline void voiceResolverIndexSelfTest_() {
    static_assert(VoiceResolver::polyVoiceSlot(2)  == 1,  "V2 → voiceSlot 1");
    static_assert(VoiceResolver::polyVoiceSlot(16) == 15, "V16 → voiceSlot 15");
    static_assert(VoiceResolver::polyVoiceSlot(1)  == -1, "mono has no poly slot");
    // voiceSlot and polyVoiceSlot are the SAME value for poly voices — the COLLAPSE unified
    // the old twin conventions into one. If anyone reintroduces a 0-based poly bank, THIS
    // fails to compile rather than misrouting CV at runtime.
    static_assert(VoiceResolver::voiceSlot(1)  == 0,  "mono is slot 0");
    static_assert(VoiceResolver::voiceSlot(2)  == 1,  "V2 is slot 1 (NOT slot 0 — the bug)");
    static_assert(VoiceResolver::voiceSlot(16) == 15, "V16 is slot 15");
    static_assert(VoiceResolver::voiceSlot(2) == VoiceResolver::polyVoiceSlot(2),
                  "voiceSlot(poly) == polyVoiceSlot(poly) — ONE convention");
    static_assert(VoiceResolver::voiceSlot(16) == VoiceResolver::polyVoiceSlot(16),
                  "voiceSlot == polyVoiceSlot across the whole poly range");
    static_assert(VoiceResolver::slotToVoice(VoiceResolver::voiceSlot(9)) == 9, "slot round-trips");
    static_assert(VoiceResolver::kVoiceSlots == 16, "16 voice slots (mono + 15 poly)");
    static_assert(VoiceResolver::kMonoSlot == VoiceResolver::voiceSlot(VoiceResolver::kMonoVoice), "kMonoSlot is the mono voice slice");
    static_assert(VoiceResolver::isMono(1),               "V1 is mono");
    static_assert(!VoiceResolver::isPoly(1),              "V1 is not poly");
    static_assert(VoiceResolver::isPoly(2) && VoiceResolver::isPoly(16), "V2..V16 poly");
    static_assert(!VoiceResolver::baseEditableHere(1),    "mono base not editable on East/Macro");
    static_assert(VoiceResolver::baseEditableHere(2),     "poly base editable");
}

} // namespace dotModular
