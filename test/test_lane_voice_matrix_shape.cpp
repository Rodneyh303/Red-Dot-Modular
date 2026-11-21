// test_lane_voice_matrix_shape — CLAIM: "7 lanes × 16 voices" — every (voice-slot, lane) cell is its OWN
// independent store: writing one never changes another (docs/design/CLAIMS_EVIDENCE.md A2).
// The aliasing class this guards is real project history: VAR/LEG once read QMIX/VAR's bank
// (LaneMapping.hpp lorStoreBank note; polyLenERef 'masks & 3 aliases VAR -> REST').
//
// On the REAL SequencerEngine/PatternEngine:
//   (1) probability rows:  polyRandom(slot, lane)[step] — 16 slots × 7 lanes × 16 steps, each written a UNIQUE
//       value, then every cell read back: no write lands in any other cell (112 independent rows);
//   (2) LOR rows: polyLenERef/OffERef/RotERef(slot, lane) — same unique-write / read-back sweep;
//   (3) the matrix really is 16 × 7: slot 15 / lane 6 are valid and distinct from slot 0 / lane 0.
#include <cstdio>
#include <set>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
using dotModular::VoiceResolver; using LaneId = dotModular::LaneId;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { if (g_fail < 40) std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static const int kLanes[7] = {
    SequencerEngine::PL_MELODY, SequencerEngine::PL_OCTAVE, SequencerEngine::PL_QMIX,
    SequencerEngine::PL_REST,   SequencerEngine::PL_ACCENT, SequencerEngine::PL_VARIATION,
    SequencerEngine::PL_LEGATO };

int main() {
    static SequencerEngine e;
    e.reset(); e.numPolyVoices = 15;
    char msg[160];
    // all 7 PL_* lane ids are distinct
    { std::set<int> ids(kLanes, kLanes + 7); CHECK(ids.size() == 7, "the seven lane ids are not distinct"); }

    const int kSlots = dotModular::VoiceResolver::kVoiceSlots;
    CHECK(kSlots == 16, "voice-slot count is not 16");
    auto tag = [](int slot, int lane, int step) { return 0.001f + 0.0001f * (slot * 7 * 16 + lane * 16 + step); };

    // (1) probability rows
    for (int s = 0; s < kSlots; ++s) for (int l = 0; l < 7; ++l) for (int i = 0; i < 16; ++i)
        e.pe.polyRandom(VoiceResolver::slot(s + 1), LaneId{kLanes[l]})[i] = tag(s, l, i);
    int bad = 0;
    for (int s = 0; s < kSlots; ++s) for (int l = 0; l < 7; ++l) for (int i = 0; i < 16; ++i)
        if (e.pe.polyRandom(VoiceResolver::slot(s + 1), LaneId{kLanes[l]})[i] != tag(s, l, i)) ++bad;
    std::snprintf(msg, sizeof msg, "probability matrix aliasing: %d of %d cells hold another cell's value", bad, kSlots * 7 * 16);
    CHECK(bad == 0, msg);

    // (2) LOR rows
    auto lv = [](int s, int l, int item) { return 1 + (s * 7 + l) * 3 + item; };   // unique 1..336
    for (int s = 0; s < kSlots; ++s) for (int l = 0; l < 7; ++l) {
        e.polyLORRef(VoiceResolver::slot(s + 1), LaneId{l}, SequencerEngine::LOR_LEN) = lv(s, l, 0);
        e.polyLORRef(VoiceResolver::slot(s + 1), LaneId{l}, SequencerEngine::LOR_OFF) = lv(s, l, 1);
        e.polyLORRef(VoiceResolver::slot(s + 1), LaneId{l}, SequencerEngine::LOR_ROT) = lv(s, l, 2);
    }
    bad = 0;
    for (int s = 0; s < kSlots; ++s) for (int l = 0; l < 7; ++l) {
        if (e.polyLOR(VoiceResolver::slot(s + 1), LaneId{l}, SequencerEngine::LOR_LEN) != lv(s, l, 0)) ++bad;
        if (e.polyLOR(VoiceResolver::slot(s + 1), LaneId{l}, SequencerEngine::LOR_OFF) != lv(s, l, 1)) ++bad;
        if (e.polyLOR(VoiceResolver::slot(s + 1), LaneId{l}, SequencerEngine::LOR_ROT) != lv(s, l, 2)) ++bad;
    }
    std::snprintf(msg, sizeof msg, "LOR matrix aliasing: %d of %d values hold another cell's value", bad, kSlots * 7 * 3);
    CHECK(bad == 0, msg);

    if (g_fail) { std::printf("test_lane_voice_matrix_shape: %d FAILED\n", g_fail); return 1; }
    std::printf("test_lane_voice_matrix_shape: 16 slots x 7 lanes — %d probability cells and %d LOR values all independent\n",
                kSlots * 7 * 16, kSlots * 7 * 3);
    return 0;
}
