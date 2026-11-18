// test/test_uas_abs1_mask_ring.cpp — CC_PROMPT_UAS_ABS1_AND_MASK_RING
// (implements the abs1 + painted-overlay rulings from
// CC_FIX_GLOBAL_BELOW_VOICE_PHASE_DESYNC.md — the A-series and P-series tables
// are the truth; this suite encodes them as assertions).
//
// PART A (abs1): the ring = the CURRENT GLOBAL PHRASE. Sounds at phrase steps
// in [O, O+L) on the G-ring (gate = offset+length, rotation ABSENT); content at
// a sounding step s = (s + R − O) mod G (the region start plays cell R). A pure
// function of (phraseStep mod G, O, L) — A7 pins the drift-free sweep.
//
// PART B (the painted overlay): ABSOLUTE on the G-ring — indexed by the phrase
// step, rotated by Δ = R − anchorR with rotMaskRing on G (a painted mute at
// step k lands at (k − Δ) mod G); the storage stays 32-wide with the bits
// beyond G DORMANT (preserved; they reappear when G grows). abs2 (followR off)
// is static/R-invariant; abs3 (followR on) travels with the content.
//
// GUARDRAILS (the two traps, not re-sprung): P2's per-repetition divergence is
// THE FEATURE — assert the shape (the mask-determined mutes over the same
// loop), never byte-equality across iterations; and no raw cross-engine A==B —
// abs1 and the overlay are stateless pure functions of the current G, asserted
// within one engine.
#include <cstdio>
#include <cstdint>
#include <vector>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;
static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static const auto V2S = VoiceResolver::slot(2);

struct StepObs { int phrasePos; bool sounding; int content; };

static std::vector<StepObs> march(SequencerEngine& eng, int steps) {
    std::vector<StepObs> obs;
    for (int n = 0; n < steps; ++n) {
        eng.advancePlayhead(1, nullptr);
        obs.push_back({ eng.pos_.phrasePos(),
                        eng.outputMaskPasses(V2S, eng.pos_.phrasePos()),
                        (int)(eng.voiceDriver_[V2S.v] - 1) });
    }
    return obs;
}

// The shared rig: a Local-East V2 row (L/O/R), the base kind, and the painted
// overlay (the anchored mask + the anchor R + the rotation source).
static void rig(SequencerEngine& eng, int G, int L, int O, int R,
                bool abs1, uint32_t mask, bool followR) {
    eng.reset();
    eng.updateWindow((float)G, 0.f, false, 0.f, 0.f, false);
    eng.numPolyVoices = 2;
    eng.setVoiceLorLocalEast(V2S, true);
    eng.voiceLorRef(V2S, SequencerEngine::LOR_LEN) = L;
    eng.voiceLorRef(V2S, SequencerEngine::LOR_OFF) = O;
    eng.voiceLorRef(V2S, SequencerEngine::LOR_ROT) = R;
    eng.maskLocalEast_[V2S.v] = true;
    eng.maskKind_[V2S.v] = abs1 ? SequencerEngine::MASK_DYNAMIC : SequencerEngine::MASK_CYCLE;
    eng.maskAnchored_[V2S.v] = mask;
    eng.maskAnchorR_[V2S.v] = 0;        // the paint was made at base R = 0
    eng.maskFollowR_[V2S.v] = followR;
}

// ── PART A: the abs1 A-series ────────────────────────────────────────────────
static void a_series() {
    std::printf("A-series: abs1 on the current-global-phrase ring\n");
    char msg[200];
    struct Cfg { int G, O, L, R; bool abs1; const char* tag; };
    const Cfg cfgs[] = {
        {8, 2, 4, 0, true,  "A1"},
        {8, 2, 4, 2, true,  "A2"},
        {4, 0, 8, 0, true,  "A4"},
        {8, 0, 4, 0, true,  "A6-abs1"},
        {8, 0, 4, 0, false, "A6-cycle"},
    };
    for (const Cfg& c : cfgs) {
        SequencerEngine eng;
        rig(eng, c.G, c.L, c.O, c.R, c.abs1, 0xFFFFFFFFu, false);
        // The declared L stays as authored — no cached min (A4's second point).
        int dL, dO, dR; SequencerEngine::LaneDir dd;
        eng.effectiveVoiceLor(V2S.v, dL, dO, dR, dd);
        std::snprintf(msg, sizeof msg, "%s: the declared L was clamped (%d, want %d) — the extent is a consequence, never a stored min", c.tag, dL, c.L);
        CHECK(dL == c.L, msg);
        const auto obs = march(eng, c.G);
        for (int s = 0; s < c.G; ++s) {
            const bool wantSound = !c.abs1 || (c.L >= c.G) || ((((s - c.O) % c.G) + c.G) % c.G < c.L);
            std::snprintf(msg, sizeof msg, "%s: phrase step %d sounds=%d, want %d (G=%d O=%d L=%d)", c.tag, s, (int)obs[s].sounding, (int)wantSound, c.G, c.O, c.L);
            CHECK(obs[s].sounding == wantSound, msg);
            if (c.abs1) {
                const int wantContent = ((((s + c.R - c.O) % c.G) + c.G) % c.G);
                std::snprintf(msg, sizeof msg, "%s: phrase step %d content %d, want %d (the rho=(R-O) anchor: content at s=O is exactly R)", c.tag, s, obs[s].content, wantContent);
                CHECK(obs[s].content == wantContent, msg);
            } else {
                const int wantLoop = ((((s + c.R) % c.L) + c.L) % c.L);
                std::snprintf(msg, sizeof msg, "%s: phrase step %d content %d, want %d (the cycle row LOOPS its window)", c.tag, s, obs[s].content, wantLoop);
                CHECK(obs[s].content == wantLoop, msg);
            }
        }
    }
    {   // A7: abs1 O0 L4, G 8->4 — the sounding set is a pure function of the
        // CURRENT G, zero residue: before {0..3} sound / {4..7} silent; after ALL
        // 4 (L >= G), immediately; the declared L stays 4 throughout.
        SequencerEngine eng;
        rig(eng, 8, 4, 0, 0, true, 0xFFFFFFFFu, false);
        auto obs = march(eng, 8);
        for (int s = 0; s < 8; ++s) {
            const bool want = s < 4;
            std::snprintf(msg, sizeof msg, "A7-before: phrase step %d sounds=%d, want %d (G8 O0 L4: 0-3 sound, 4-7 silent)", s, (int)obs[s].sounding, (int)want);
            CHECK(obs[s].sounding == want, msg);
        }
        eng.updateWindow(4.f, 0.f, false, 0.f, 0.f, false);
        obs = march(eng, 4);
        for (int s = 0; s < 4; ++s) {
            std::snprintf(msg, sizeof msg, "A7-after: phrase step %d silent after G 8->4 (L=4 >= G=4 — the whole phrase sounds, immediately, drift-free)", s);
            CHECK(obs[s].sounding, msg);
        }
    }
}

// ── PART B: the painted-overlay P-series ─────────────────────────────────────
static void p_series() {
    std::printf("P-series: the painted overlay, absolute on the G-ring\n");
    char msg[200];
    // The doc's mask strings are STEP-0-LEFT; the bit k of the authored mask is
    // the phrase step k. Every want is computed from the OBSERVED phrase
    // position (robust to the phrase sequence continuing across window changes).
    auto maskBit = [](uint32_t m, int step) { return ((m >> step) & 1u) != 0; };
    {   // P1: cycle L4 G8, mask 1110 1110 (mute 3,7 — the mask's period 4 == L):
        // every loop iteration IDENTICAL (absolute reproduces the window-rider).
        const uint32_t M = 0b0111'0111u;   // steps 3 and 7 muted
        SequencerEngine eng;
        rig(eng, 8, 4, 0, 0, false, M, false);
        const auto obs = march(eng, 8);
        for (int i = 0; i < 8; ++i) {
            const int s = obs[i].phrasePos;
            const bool want = maskBit(M, s);
            std::snprintf(msg, sizeof msg, "P1: phrase step %d sounds=%d, want %d (the mask ANDs the cycle base)", s, (int)obs[i].sounding, (int)want);
            CHECK(obs[i].sounding == want, msg);
        }
        for (int s = 0; s < 4; ++s) {
            std::snprintf(msg, sizeof msg, "P1: mask period == L — iteration 2 (step %d) must be IDENTICAL to iteration 1 (sounding %d vs %d, content %d vs %d)", s + 4, (int)obs[s + 4].sounding, (int)obs[s].sounding, obs[s + 4].content, obs[s].content);
            CHECK(obs[s].sounding == obs[s + 4].sounding && obs[s].content == obs[s + 4].content, msg);
        }
    }
    {   // P2: mask 1110 1101 (mute 3,6 — period 8 != L): the SAME loop under
        // DIFFERENT mutes each repetition — the phasing / varied-repetition the
        // absolute overlay exists for. The iterations MUST differ (the feature).
        const uint32_t M = 0b1011'0111u;   // steps 3 and 6 muted
        SequencerEngine eng;
        rig(eng, 8, 4, 0, 0, false, M, false);
        const auto obs = march(eng, 8);
        for (int i = 0; i < 8; ++i) {
            const int s = obs[i].phrasePos;
            const bool want = maskBit(M, s);
            std::snprintf(msg, sizeof msg, "P2: phrase step %d sounds=%d, want %d (the mask ANDs, absolute on the G-ring)", s, (int)obs[i].sounding, (int)want);
            CHECK(obs[i].sounding == want, msg);
        }
        bool differ = false;
        for (int s = 0; s < 4; ++s) if (obs[s].sounding != obs[s + 4].sounding) differ = true;
        CHECK(differ, "P2: period != L — the two loop iterations MUST differ (the phasing feature, not a failure)");
        bool sameLoop = true;
        for (int s = 0; s < 4; ++s) if (obs[s].content != obs[s + 4].content) sameLoop = false;
        CHECK(sameLoop, "P2: the CONTENT loop is the same under both iterations (the same loop, different mutes)");
    }
    {   // P3: mask 1101 1111 (mute 5... the doc's mute step 2 -> 0b1111'1011), R 0->1:
        // abs2 keeps the muted step R-INVARIANT; abs3 moves it to (k - Δ) mod G.
        SequencerEngine eng2;
        rig(eng2, 8, 8, 0, 1, false, 0b1111'1011u, false);   // abs2: followR OFF, base R = 1
        const auto o2 = march(eng2, 8);
        int mute2 = -1; for (int s = 0; s < 8; ++s) if (!o2[s].sounding) mute2 = s;
        std::snprintf(msg, sizeof msg, "P3 abs2: the muted step is %d, want 2 (R-INVARIANT — the anchored copy stands)", mute2);
        CHECK(mute2 == 2, msg);
        SequencerEngine eng3;
        rig(eng3, 8, 8, 0, 1, false, 0b1111'1011u, true);    // abs3: followR ON, Δ = R - anchorR = 1
        const auto o3 = march(eng3, 8);
        int mute3 = -1; for (int s = 0; s < 8; ++s) if (!o3[s].sounding) mute3 = s;
        std::snprintf(msg, sizeof msg, "P3 abs3: the muted step is %d, want 1 ((k - Δ) mod G = 2 - 1 — the mute travels with the content's rotation)", mute3);
        CHECK(mute3 == 1, msg);
    }
    {   // P4: abs1 region [2,6) + mask 1111 0111 (mute 4): sounds {2,3,5} — the
        // overlay ANDs the abs1 gate => NON-CONTIGUOUS sounding inside the region.
        const uint32_t M = 0b1110'1111u;   // step 4 muted
        SequencerEngine eng;
        rig(eng, 8, 4, 2, 0, true, M, false);
        const auto obs = march(eng, 8);
        for (int i = 0; i < 8; ++i) {
            const int s = obs[i].phrasePos;
            const bool want = (s == 2 || s == 3 || s == 5);
            std::snprintf(msg, sizeof msg, "P4: phrase step %d sounds=%d, want %d (the region [2,6) AND the mute at 4 — non-contiguous sounding, unreachable by either base alone)", s, (int)obs[i].sounding, (int)want);
            CHECK(obs[i].sounding == want, msg);
        }
    }
    {   // P5: the mask authored on G8 (mute 5), then G 8->4: the overlay uses
        // bits 0-3 only (the mute at 5 is DORMANT); grow back — it reappears.
        // Zero residue: the sounding set is a pure function of (phraseStep mod G,
        // mask, R).
        const uint32_t M = 0b1101'1111u;   // step 5 muted
        SequencerEngine eng;
        rig(eng, 8, 4, 0, 0, false, M, false);
        auto obs = march(eng, 8);
        for (int i = 0; i < 8; ++i) {
            const int s = obs[i].phrasePos;
            const bool want = maskBit(M, s);
            std::snprintf(msg, sizeof msg, "P5-at-G8: phrase step %d sounds=%d, want %d (the authored mask on the 8-ring)", s, (int)obs[i].sounding, (int)want);
            CHECK(obs[i].sounding == want, msg);
        }
        eng.updateWindow(4.f, 0.f, false, 0.f, 0.f, false);
        obs = march(eng, 4);
        for (int i = 0; i < 4; ++i) {
            const int s = obs[i].phrasePos;
            std::snprintf(msg, sizeof msg, "P5-at-G4: phrase step %d muted (bit 5 is DORMANT — beyond the G-ring, preserved on the 32-wide storage)", s);
            CHECK(obs[i].sounding, msg);
        }
        eng.updateWindow(8.f, 0.f, false, 0.f, 0.f, false);
        obs = march(eng, 8);
        for (int i = 0; i < 8; ++i) {
            const int s = obs[i].phrasePos;
            const bool want = maskBit(M, s);
            std::snprintf(msg, sizeof msg, "P5-regrown: phrase step %d sounds=%d, want %d (the dormant bit REAPPEARS when G grows back — zero residue)", s, (int)obs[i].sounding, (int)want);
            CHECK(obs[i].sounding == want, msg);
        }
    }
    {   // P5b: the rotation wraps on the G-ring — a mute at step 0 with Δ=1
        // under G=4 lands at (0 − 1) mod 4 = 3, NOT off the phrase (the 32-ring
        // rotation would push it to bit 31, invisible — the discriminator).
        SequencerEngine eng;
        rig(eng, 4, 4, 0, 1, false, 0b1111'1110u, true);    // mute at 0, abs3, Δ = 1
        const auto obs = march(eng, 4);
        int mute = -1; for (int s = 0; s < 4; ++s) if (!obs[s].sounding) mute = s;
        std::snprintf(msg, sizeof msg, "P5b: the rotated muted step is %d, want 3 ((0 - 1) mod 4 — rotMaskRing wraps on the G-ring)", mute);
        CHECK(mute == 3, msg);
    }
}

int main() {
    std::printf("test_uas_abs1_mask_ring — CC_PROMPT_UAS_ABS1_AND_MASK_RING (the A/P tables)\n");
    a_series();
    p_series();
    if (g_fail == 0) std::printf("all green\n");
    else std::printf("%d FAILED\n", g_fail);
    return g_fail ? 1 : 0;
}
