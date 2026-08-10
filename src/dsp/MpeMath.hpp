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

} // namespace mpe
} // namespace dotModular
