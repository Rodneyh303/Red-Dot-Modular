#pragma once
// ── Voice Resolver ──────────────────────────────────────────────────────────────
// A uniform 16-voice ADDRESSING layer over the engine, per docs/design/
// VOICE_RESOLVER_SPEC.md. Voice 1 = the standalone mono master strand; voices 2..16 =
// the 15 poly voices (parameterised re-derivations of mono).
//
// STEP 1 — READ-ONLY SHADOW. This layer GENERATES NOTHING and CHANGES NO BEHAVIOUR. It
// is a thin read facade over accessors that already exist on SequencerEngine:
//   voice 1  → masterLaneProbability / masterLaneStep   (mono)
//   voice ≥2 → polyLaneProbability   / polyLaneStep      (poly, bank index = voice-2)
// It exists so callers (UI tabs, prob-outs, modulation) can eventually address all 16
// voices through ONE interface instead of the scattered mono-vs-poly bridging (the -1
// remap, the tab-1 mirror special-cases, the cover-paints). Until callers are migrated
// (later, verified steps), nothing reads through this — so adding it is behaviour-inert
// and a build with it present must be bit-identical to one without.
//
// Voice numbering (matches the UI tabs V1..V16 and the channel-1-reserved convention):
//   V = 1        → mono master strand (the core Monsoon voice; standalone)
//   V = 2..16    → poly voice, poly bank index = V - 2  (0..14)
//
// The resolver only READS. Mono's generation (the Monsoon/Sands path) and poly's
// generation (executePolyVoice) are untouched; this wraps their outputs.

#include "engines/SequencerEngine.hpp"
#include "SpreadInterp.hpp"   // for SpreadInterp::Target (spread mode metadata)

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
    // Poly bank index (0..14) for V2..V16. Undefined (-1) for the mono voice — callers
    // must gate on isPoly() and never index a bank with the mono voice.
    static constexpr int  polyBankIndex(int v) { return isPoly(v) ? (v - kFirstPoly) : -1; }

    // Is the per-lane BASE editable on a poly-aware module (East/Macro) for this voice?
    // FALSE for mono: voice 1's base belongs to the standalone Mono module — East/Macro
    // mirror it read-only. (This is what tab1MonoMirror / the V1 cover-paints encode
    // today; the resolver makes it one predicate.)
    static constexpr bool baseEditableHere(int v) { return isPoly(v); }

    // Spread interpolation target for this voice: mono self-targets (fixed anchor),
    // poly converges toward the ensemble average. Already a SpreadInterp parameter, not
    // a separate algorithm — surfaced here as per-voice metadata.
    static redDot::SpreadInterp::Target spreadMode(int v) {
        return isMono(v) ? redDot::SpreadInterp::MONO_DRAW
                         : redDot::SpreadInterp::AVERAGE_POLY;
    }

    // ── Uniform per-lane reads (the point of the layer) ─────────────────────────
    // lane: SequencerEngine::PL_REST / PL_MELODY / PL_OCTAVE (0/1/2).
    // Dispatches V1 → master strand, V2..V16 → the poly bank. Bit-identical to what
    // callers compute today by hand; this just removes the per-call branching.

    // Resolved probability draw (0..1) for (voice, lane) at the voice's own LOR step.
    float laneProbability(int v, int lane) const {
        if (isMono(v)) return eng.masterLaneProbability(lane);
        if (isPoly(v)) return eng.polyLaneProbability(lane, polyBankIndex(v));
        return 0.f;
    }

    // Resolved step index (for S&H edge detection) for (voice, lane).
    int laneStep(int v, int lane) const {
        if (isMono(v)) return eng.masterLaneStep(lane);
        if (isPoly(v)) return eng.polyLaneStep(lane, polyBankIndex(v));
        return -1;
    }

    // Probability sampled at an EXPLICIT step (e.g. Macro's prob-out samples every voice
    // at Macro's own global LOR step). Mono ignores the explicit step (single strand).
    float laneProbabilityAtStep(int v, int lane, int step) const {
        if (isMono(v)) return eng.masterLaneProbability(lane);
        if (isPoly(v)) return eng.polyLaneProbabilityAtStep(lane, polyBankIndex(v), step);
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
    static_assert(VoiceResolver::polyBankIndex(2)  == 0,  "V2 → bank 0");
    static_assert(VoiceResolver::polyBankIndex(16) == 14, "V16 → bank 14");
    static_assert(VoiceResolver::polyBankIndex(1)  == -1, "mono has no bank slot");
    static_assert(VoiceResolver::isMono(1),               "V1 is mono");
    static_assert(!VoiceResolver::isPoly(1),              "V1 is not poly");
    static_assert(VoiceResolver::isPoly(2) && VoiceResolver::isPoly(16), "V2..V16 poly");
    static_assert(!VoiceResolver::baseEditableHere(1),    "mono base not editable on East/Macro");
    static_assert(VoiceResolver::baseEditableHere(2),     "poly base editable");
}

} // namespace dotModular
