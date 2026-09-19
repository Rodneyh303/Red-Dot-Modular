#pragma once
// ── dotModular::mpe — pure CV→MPE conversion math (no Rack) ───────────────────────────────────────
// The note/bend split used by the Keppel CV→MPE-out utility (MPE_UTILITY_BUILD_SPEC). Split out as a
// header-only, stdlib-only unit so it's unit-testable standalone (test/test_MpeMath.cpp) and the module
// (src/Keppel.cpp) shares the EXACT same math — no divergence between what's tested and what ships.
//
// Convention (MICROTONAL_MIDI_MPE_DIRECTION): 0V = C4 = MIDI 60. NEAREST 12-TET note + centred bend, so
// |offset| <= 0.5 semitone; a narrow bend range (default ±2 semitones) then never clips and gives
// sub-cent resolution. The microtonal-ness is already in the voltage — tuning-agnostic by construction.

#include <cmath>
#include <algorithm>

namespace dotModular {
namespace mpe {

// Nearest 12-TET MIDI note for a 1V/oct pitch voltage (0V→60). Clamped to the MIDI range.
inline int noteFor(float pitchV) {
    int n = (int)std::lround((double)pitchV * 12.0) + 60;
    return n < 0 ? 0 : (n > 127 ? 127 : n);
}

// Signed within-semitone offset (in SEMITONES) from that nearest note: pitchV*12 - round(pitchV*12).
// Range [-0.5, +0.5]. This is what the per-note pitch bend must encode.
inline float centsOffsetSemis(float pitchV) {
    double semis = (double)pitchV * 12.0;
    return (float)(semis - std::lround(semis));
}

// 14-bit pitch-bend value (0..16383, centre 8192) for a given within-semitone offset and the receiver's
// bend range (semitones each side). offset/​range maps to ±8192 around centre. Clamped to [0,16383].
// bendRangeSemis is clamped to >=1 to avoid divide-by-zero / absurd scaling.
inline int bend14(float offsetSemis, float bendRangeSemis) {
    if (bendRangeSemis < 1.f) bendRangeSemis = 1.f;
    int v = (int)std::lround(8192.0 + (double)offsetSemis * (8192.0 / (double)bendRangeSemis));
    return v < 0 ? 0 : (v > 16383 ? 16383 : v);
}

// Convenience: 14-bit bend straight from a pitch voltage + range (nearest-note offset internally).
inline int bend14For(float pitchV, float bendRangeSemis) {
    return bend14(centsOffsetSemis(pitchV), bendRangeSemis);
}

// Signed offset (SEMITONES) from a FIXED, already-latched MIDI note (0V=60): pitchV*12 - (note-60).
// Used for continuous bend tracking while a voice is held — the MIDI note stays put and only the bend
// moves, so glides/vibrato within ±bendRange play smoothly (no re-articulation). Unlike centsOffsetSemis
// this is NOT bounded to ±0.5: as the pitch drifts from the note the offset grows, and bend14 clamps at
// the range edge (a slide past ±bendRange holds at the extreme rather than wrapping).
inline float offsetFromNoteSemis(float pitchV, int note) {
    return (float)((double)pitchV * 12.0 - (double)(note - 60));
}

// 14-bit bend for a held voice tracking pitchV against its latched note + the receiver's bend range.
inline int bend14FromNote(float pitchV, int note, float bendRangeSemis) {
    return bend14(offsetFromNoteSemis(pitchV, note), bendRangeSemis);
}

// Reconstruct the 1V/oct pitch an IDEAL MPE receiver reproduces from a latched note + 14-bit bend and
// the receiver's bend range: the inverse of (noteFor + bend14…). Used by Keppel's reverse-calc MONITOR
// output (internal ground truth for the round-trip test) and by test/test_MpeMath.cpp. Because bend14
// is 14-bit, the round-trip error is bounded by half a bend step = bendRange/16384 semitone
// ≈ bendRange·0.0061 cents (≈0.29 cents even at ±48), so a correct split reconstructs sub-cent at ANY
// range in 1..48 — which is what makes widening the range safe. Note: if the ORIGINAL bend clamped
// (|offset| > bendRange, the legato landmine), the reconstruction saturates at note ± bendRange and does
// NOT match the input — that mismatch is exactly what the re-articulation fix removes.
inline float reconstructVolts(int note, int bend14, float bendRangeSemis) {
    if (bendRangeSemis < 1.f) bendRangeSemis = 1.f;
    double offsetSemis = ((double)bend14 - 8192.0) * ((double)bendRangeSemis / 8192.0);
    double semis = (double)(note - 60) + offsetSemis;
    return (float)(semis / 12.0);
}

} // namespace mpe
} // namespace dotModular
