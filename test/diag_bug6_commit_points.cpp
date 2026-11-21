// diag_bug6_commit_points.cpp — REPRODUCE-FIRST diagnostic for BUG 6.
//
// Rodney's report: "modulated offset range doesn't move always at voice wrap;
// sometimes yes, sometimes no, sometimes at other points in voice travel."
// And visually: "voice lane wraps, advances 1 step, THEN the modulation impacts
// the probability lane."
//
// This is NOT an assertion suite. It drives the exact scene (8-voice frame, a
// 3-length rhythm probability lane — the 3-against-8 tresillo) and PRINTS, per
// tick: the true voice-frame wrap (cell==0), the committed lane offset, and the
// lane read. We want to SEE whether the lane commit coincides with the wrap.
//
//   Scene A: static voice period (global=8). Offset CV pushed once.
//   Scene B: global window length modulated each cycle (8<->7) — the thing
//            Rodney flagged ("impact of global range on voice range"): does the
//            voice-frame period move, and does the lane commit chase it?
#include <cstdio>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;
using LaneId = dotModular::LaneId;
static dotModular::VoiceSlot v2() { return VoiceResolver::slot(2); }
static constexpr int STR = dotModular::STRAND_RHYTHM;

static void dump_header(const char* title) {
    std::printf("\n=== %s ===\n", title);
    std::printf(" t | pp phase  cell(wrap?) | commO  read | notes\n");
    std::printf("---+---------------------- +-------------+------\n");
}

// Read the committed lane {LEN,OFF} for PL_REST on this voice.
static void committedLaneLO(SequencerEngine& eng, int& L, int& O) {
    int R; SequencerEngine::LaneDir d;
    eng.effectiveLaneLor(v2(), LaneId{SequencerEngine::PL_REST}, L, O, R, d);
}
static int committedLaneO(SequencerEngine& eng) { int L,O; committedLaneLO(eng,L,O); return O; }

// The TRUE voice-frame wrap: the cell the reads consume (laneTickFor(dir, phase+1, L)).
static int trueCell(SequencerEngine& eng) {
    int rL, rO, rR; SequencerEngine::LaneDir rd;
    eng.effectiveVoiceLor(v2().v, rL, rO, rR, rd);
    return (int)SequencerEngine::laneTickFor(rd, (int64_t)eng.voiceFramePhase_[v2().v] + 1, rL);
}

static void sceneA_static_period() {
    dump_header("Scene A: static global=8, voice len 8, rhythm lane len 3, offset CV +2 at t=1");
    SequencerEngine eng; eng.reset();
    eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
    eng.numPolyVoices = 2;
    eng.setVoiceLorLocalEast(v2(), true);
    eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 8;   // voice frame = 8
    eng.polyLORRef(v2(), LaneId{SequencerEngine::PL_REST}, SequencerEngine::LOR_LEN) = 3; // tresillo lane
    int prevCommO = committedLaneO(eng), prevRead = eng.laneTickV_[v2().v][STR];
    for (int t = 0; t < 32; ++t) {
        eng.advancePlayhead(1, nullptr);
        if (t == 1) { int d3[3] = {0, 2, 0}; eng.pushLaneLorCvPoly(v2(), SequencerEngine::PL_REST, d3); }
        const int pp = eng.pos_.phrasePos();
        const int ph = eng.voiceFramePhase_[v2().v];
        const int cell = trueCell(eng);
        const int commO = committedLaneO(eng);
        const int read = eng.laneTickV_[v2().v][STR];
        const bool wrap = (cell == 0);
        const bool commChanged = (commO != prevCommO);
        const bool readChanged = (read != prevRead);
        char note[80] = "";
        if (wrap && commChanged) std::snprintf(note, sizeof note, "commit @WRAP  <-- correct");
        else if (commChanged)    std::snprintf(note, sizeof note, "COMMIT OFF-WRAP  <-- BUG");
        else if (wrap)           std::snprintf(note, sizeof note, "wrap (no commit)");
        if (readChanged && note[0]==0) std::snprintf(note, sizeof note, "read moved");
        std::printf("%2d | %2d  %3d   %3d %-5s | %4d  %4d | %s\n",
                    t, pp, ph, cell, wrap ? "WRAP" : "", commO, read, note);
        prevCommO = commO; prevRead = read;
    }
}

static void sceneB_modulated_global() {
    dump_header("Scene B: global length modulated 8<->7 each phrase, voice len 8, lane len 3, offset CV +2 at t=1");
    SequencerEngine eng; eng.reset();
    eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
    eng.numPolyVoices = 2;
    eng.setVoiceLorLocalEast(v2(), true);
    eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 8;
    eng.polyLORRef(v2(), LaneId{SequencerEngine::PL_REST}, SequencerEngine::LOR_LEN) = 3;
    int prevCommO = committedLaneO(eng);
    float glen = 8.f;
    for (int t = 0; t < 40; ++t) {
        // flip the global length every 8 ticks (simulates the LFO on global range)
        if (t > 0 && (t % 8) == 0) { glen = (glen == 8.f) ? 7.f : 8.f; eng.updateWindow(glen, 0.f, false, 0.f, 0.f, false); }
        eng.advancePlayhead(1, nullptr);
        if (t == 1) { int d3[3] = {0, 2, 0}; eng.pushLaneLorCvPoly(v2(), SequencerEngine::PL_REST, d3); }
        const int pp = eng.pos_.phrasePos();
        const int ph = eng.voiceFramePhase_[v2().v];
        const int cell = trueCell(eng);
        const int commO = committedLaneO(eng);
        const int read = eng.laneTickV_[v2().v][STR];
        const bool wrap = (cell == 0);
        const bool commChanged = (commO != prevCommO);
        char note[80] = "";
        if (commChanged && wrap)      std::snprintf(note, sizeof note, "commit @WRAP");
        else if (commChanged)         std::snprintf(note, sizeof note, "COMMIT OFF-WRAP  <-- scatter");
        else if (wrap)                std::snprintf(note, sizeof note, "wrap");
        std::printf("%2d | %2d  %3d   %3d %-5s | %4d  %4d | glen=%.0f %s\n",
                    t, pp, ph, cell, wrap ? "WRAP" : "", commO, read, glen, note);
        prevCommO = commO;
    }
}

// Scene C: a MASK_DYNAMIC voice reads the FULL phrase (frame period = kMaskRing=32,
// a CONSTANT). Modulate the global length — does the latch period move? (Rodney's
// "impact of global range on voice range" question, answered empirically.)
static void sceneC_fullphrase_global_mod() {
    dump_header("Scene C: MASK_DYNAMIC voice (full-phrase frame), global length modulated 8<->6, lane len 3, offset CV +2 at t=1");
    SequencerEngine eng; eng.reset();
    eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
    eng.numPolyVoices = 2;
    eng.setVoiceLorLocalEast(v2(), true);
    eng.maskLocalEast_[v2().v] = true;
    eng.maskKind_[v2().v] = SequencerEngine::MASK_DYNAMIC;   // voiceFrameReadsFullPhrase=true
    eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 8;      // voice window, but frame reads the full ring
    eng.polyLORRef(v2(), LaneId{SequencerEngine::PL_REST}, SequencerEngine::LOR_LEN) = 3;
    int prevCommO = committedLaneO(eng);
    float glen = 8.f;
    for (int t = 0; t < 40; ++t) {
        if (t > 0 && (t % 8) == 0) { glen = (glen == 8.f) ? 6.f : 8.f; eng.updateWindow(glen, 0.f, false, 0.f, 0.f, false); }
        eng.advancePlayhead(1, nullptr);
        if (t == 1) { int d3[3] = {0, 2, 0}; eng.pushLaneLorCvPoly(v2(), SequencerEngine::PL_REST, d3); }
        const int cell = trueCell(eng), commO = committedLaneO(eng);
        const bool wrap = (cell == 0), cc = (commO != prevCommO);
        char note[80] = "";
        if (cc && wrap) std::snprintf(note, sizeof note, "commit @WRAP");
        else if (cc)    std::snprintf(note, sizeof note, "COMMIT OFF-WRAP  <-- scatter");
        else if (wrap)  std::snprintf(note, sizeof note, "wrap");
        std::printf("%2d | %2d  %3d   %3d %-5s | %4d  %4d | glen=%.0f %s\n",
                    t, eng.pos_.phrasePos(), eng.voiceFramePhase_[v2().v], cell, wrap?"WRAP":"", commO, eng.laneTickV_[v2().v][STR], glen, note);
        prevCommO = commO;
    }
}

// Scene D: modulate the VOICE'S OWN length (LOR_LEN) via CV — this genuinely moves
// the frame period, so the commit edge MUST move with it. This is the only input
// that relocates the probability-lane commit (and it is CORRECT that it does).
static void sceneD_voice_len_mod() {
    dump_header("Scene D: voice OWN length CV'd 8<->5 each cycle, lane len 3, offset CV +2 at t=1 (period genuinely moves)");
    SequencerEngine eng; eng.reset();
    eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
    eng.numPolyVoices = 2;
    eng.setVoiceLorLocalEast(v2(), true);
    eng.voiceLorRef(v2(), SequencerEngine::LOR_LEN) = 8;
    eng.polyLORRef(v2(), LaneId{SequencerEngine::PL_REST}, SequencerEngine::LOR_LEN) = 3;
    int prevCommO = committedLaneO(eng), prevVL = 8;
    for (int t = 0; t < 40; ++t) {
        // CV the voice length via the east LOR delta (len axis) every 8 ticks
        if (t > 0 && (t % 8) == 0) {
            const int newLen = (prevVL == 8) ? 5 : 8;
            std::vector<int> m3 = {0,0,0}; std::vector<int> east(16*3, 0);
            east[v2().v * 3 + SequencerEngine::LOR_LEN] = newLen - 8;   // delta from base 8
            eng.pushVoiceLorCv(m3.data(), east.data());
            prevVL = newLen;
        }
        eng.advancePlayhead(1, nullptr);
        if (t == 1) { int d3[3] = {0, 2, 0}; eng.pushLaneLorCvPoly(v2(), SequencerEngine::PL_REST, d3); }
        int vL, vO, vR; SequencerEngine::LaneDir vd; eng.effectiveVoiceLor(v2().v, vL, vO, vR, vd);
        const int cell = trueCell(eng), commO = committedLaneO(eng);
        const bool wrap = (cell == 0), cc = (commO != prevCommO);
        char note[80] = "";
        if (cc && wrap) std::snprintf(note, sizeof note, "commit @WRAP (frame moved, correct)");
        else if (cc)    std::snprintf(note, sizeof note, "COMMIT OFF the cell-0 of CURRENT period");
        else if (wrap)  std::snprintf(note, sizeof note, "wrap");
        std::printf("%2d | %2d  %3d   %3d %-5s | %4d  vL=%d | %s\n",
                    t, eng.pos_.phrasePos(), eng.voiceFramePhase_[v2().v], cell, wrap?"WRAP":"", commO, vL, note);
        prevCommO = commO;
    }
}

int main() {
    std::printf("BUG 6 reproduce-first diagnostic — commit points vs true voice-frame wrap\n");
    sceneA_static_period();
    sceneB_modulated_global();
    sceneC_fullphrase_global_mod();
    sceneD_voice_len_mod();
    std::printf("\n(diagnostic only — read the OFF-WRAP markers)\n");
    return 0;
}
