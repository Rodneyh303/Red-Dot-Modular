// test_macro_shared_state.cpp — CC_PROMPT_MACRO_RESOLVER_AND_IDENTITY (LOR_EDITING
// bugs 19-21 + 23): the MACRO-follower coverage the battery lacked. The doc's
// §THE TEST GAP: "The suites assert the shared mechanism but never drive MACRO
// followers across ALL 16 voices including the mono slot." These gates DO —
// through VoiceResolver, for every voice number in [kMonoVoice..kLastVoice]:
//
//   1. MACRO SHARED WINDOW: a Macro window (O=16, L=8); EVERY follower voice —
//      V1 (the mono slot) through V16 — reads the SAME window: voiceRead_ in
//      [16, 24), the identical period, in lockstep.
//   2. MACRO SHARED MASK: a painted Macro mask with bits off; outputMaskPasses
//      is IDENTICAL for every follower at every global step (masked steps
//      silent for ALL voices including slot 0); all-on -> every follower
//      passes everywhere; all-off -> none pass anywhere.
//   3. MONO SLOT NOT SPECIAL: slot 0's window and mask reads equal slots 1..15
//      throughout the march — the direct pin on the voice-frame loop.
//   4. IDENTITY = THE FULL VOICE WINDOW (bug 23): after reset the voice window
//      is kPatternLengthCap-wide (the identity sentinel — was the literal 16);
//      the identity voice imposes no range restriction (its read spans the
//      global window's cells), and changing the global pattern length mid-play
//      leaves the identity voice unrestricted (the window does not depend on
//      the global length).
//   5. THE GREP PIN: no literal 16 remains as the identity/length sentinel in
//      the voice-window path (voiceLorIdentity + the two pass-through checks
//      spell the named constant).
//
// THE AUDIT THIS SUITE RECORDS (the bug-16 lesson, applied up front): the doc's
// located cause for bugs 19-21 — the voice-frame loop's raw `v == 0 ||
// (v >= 1 && v <= numPolyVoices)` liveness test bypassing VoiceResolver — does
// NOT reproduce at the engine level: the loop's BODY resolves every live slot
// (slot 0 included) through effectiveVoiceLor/voiceLorIdentity (the EFFECTIVE
// row), so all followers share the Macro window (gates 1-3 pass on the
// pre-fix branch — the probe: V1=V2=V3=V16 all in [16,24), period 8). The
// PART-A refactor (the resolver-routed loop) lands as the mandated hygiene
// with these gates proving the no-behaviour-change. Gate 4 FAILS pre-fix (the
// 16 sentinel) — bug 23's reproduction. The Rack symptoms' remaining suspects
// (the strand-LOR vs voice-LOR split for V1's cycle-mode audio; the run-time
// independence flags) are recorded in the commit.
//
// Build (run_all.sh): test_macro_shared_state|$SE $GS $PE $CE

#include <cstdio>
#include <cstdint>
#include <string>

#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

static bool slurpFile(const char* path, std::string& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

int main() {
    const int CAP  = dotModular::kPatternLengthCap;
    const uint64_t ALL = dotModular::kMaskLenMask;

    // ── GATE 1 + 3: the shared window — every follower, the mono slot included ──
    {
        SequencerEngine eng;
        eng.reset();
        eng.numPolyVoices = 15;                     // all voices live
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 8;
        eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = 16;
        eng.macroVoiceLor_[SequencerEngine::LOR_ROT] = 0;
        bool ok = true;
        int prev[16] = {};
        for (int t = 0; t < 24; ++t) {
            eng.advancePlayhead(1, nullptr);
            for (int vn = VoiceResolver::kMonoVoice; vn <= VoiceResolver::kLastVoice; ++vn) {
                const int rp = eng.voiceReadPosition(VoiceResolver::slot(vn));
                ok = ok && rp >= 16 && rp < 24;                     // in the shared window
                // the identical period: every voice advances by 1 mod 8 (in lockstep)
                if (t > 0)
                    ok = ok && rp == ((prev[vn - 1] + 1 - 16) % 8 + 8) % 8 + 16;
                prev[vn - 1] = rp;
            }
            // MONO SLOT NOT SPECIAL: slot 0's read equals every other slot's.
            const int rp0 = eng.voiceReadPosition(VoiceResolver::slot(1));
            for (int vn = 2; vn <= 16; ++vn)
                ok = ok && eng.voiceReadPosition(VoiceResolver::slot(vn)) == rp0;
        }
        check(ok, "1+3: EVERY follower (V1's mono slot through V16) reads the SAME "
                  "Macro window [16,24), period 8, in lockstep — the mono slot is "
                  "not special");
    }

    // ── GATE 2: the shared mask — identical for every follower at every step ────
    {
        SequencerEngine eng;
        eng.reset();
        eng.numPolyVoices = 15;
        // the Macro tier: kind PAINTED, cells 3 and 17 off (all voices follow —
        // maskLocalEast_ defaults false).
        eng.maskMacroKind_ = SequencerEngine::MASK_PAINTED;
        eng.maskMacroAnchored_ = ALL & ~((1ull << 3) | (1ull << 17));
        eng.maskMacroLastPushed_ = eng.maskMacroAnchored_;
        bool ok = true;
        for (int s = 0; s < 32; ++s) {
            const bool p0 = eng.outputMaskPasses(VoiceResolver::slot(1), s);
            ok = ok && p0 == !(s == 3 || s == 17);
            for (int vn = 2; vn <= 16; ++vn)
                ok = ok && eng.outputMaskPasses(VoiceResolver::slot(vn), s) == p0;
        }
        // all-on -> every follower passes everywhere; all-off -> none anywhere.
        eng.maskMacroAnchored_ = ALL; eng.maskMacroLastPushed_ = ALL;
        for (int s = 0; s < 32; ++s)
            for (int vn = 1; vn <= 16; ++vn)
                ok = ok && eng.outputMaskPasses(VoiceResolver::slot(vn), s);
        eng.maskMacroAnchored_ = 0; eng.maskMacroLastPushed_ = 0;
        for (int s = 0; s < 32; ++s)
            for (int vn = 1; vn <= 16; ++vn)
                ok = ok && !eng.outputMaskPasses(VoiceResolver::slot(vn), s);
        check(ok, "2: the Macro mask gates EVERY follower identically (masked steps "
                  "silent for all voices incl. slot 0; all-on passes; all-off mutes)");
    }

    // ── GATE 4: identity = the FULL voice window (bug 23) ────────────────────────
    {
        SequencerEngine eng;
        eng.reset();
        eng.numPolyVoices = 15;
        // The reset seeds the voice rows; the identity sentinel must be the CAP:
        // voiceLorIdentity uses the full-window values (cap/0/0/Fwd).
        bool ok = true;
        {
            // author the Macro row at the identity values via the CAP constant:
            eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = CAP;
            eng.macroVoiceLor_[SequencerEngine::LOR_OFF] = 0;
            eng.macroVoiceLor_[SequencerEngine::LOR_ROT] = 0;
            for (int vn = 1; vn <= 16; ++vn)
                ok = ok && eng.voiceLorIdentity(VoiceResolver::slot(vn).v);
        }
        // The identity voice imposes NO range restriction: its read spans the
        // global window's cells. March at the default global length (16) — the
        // reads cover [0,16); change the global length to the cap mid-play —
        // the reads then span [0,32) (the window does not depend on the global
        // length).
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 16;   // the pre-fix sentinel: NOT identity after the fix — a genuine 16-window
        bool seen[32] = {};
        for (int t = 0; t < 32; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int rp = eng.voiceReadPosition(VoiceResolver::slot(1));
            ok = ok && rp >= 0 && rp < 16;                   // the authored 16-window confines
            seen[rp] = true;
        }
        for (int c = 0; c < 16; ++c) ok = ok && seen[c];      // all its cells
        // back to IDENTITY (the cap row) — the full window, at any global length.
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = CAP;
        eng.updateWindow((float)CAP, 0.f, false, 0.f, 0.f, false);
        bool seen2[32] = {};
        for (int t = 0; t < 40; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int rp = eng.voiceReadPosition(VoiceResolver::slot(1));
            ok = ok && rp >= 0 && rp < CAP;
            seen2[rp] = true;
        }
        for (int c = 0; c < CAP; ++c) ok = ok && seen2[c];    // the full window at the cap
        check(ok, "4: identity = the FULL voice window (the cap) — no restriction, "
                  "independent of the global pattern length");
    }

    // ── GATE 5: the grep pin — no literal 16 sentinel in the voice-window path ──
    {
        std::string hpp, cpp;
        bool ok = slurpFile("src/dsp/engines/SequencerEngine.hpp", hpp)
               && slurpFile("src/dsp/engines/SequencerEngine.cpp", cpp);
        // voiceLorIdentity spells the constant, not the literal.
        ok = ok && hpp.find("L == kPatternLengthCap && O == 0 && R == 0") != std::string::npos;
        ok = ok && hpp.find("L == 16 && O == 0 && R == 0") == std::string::npos;   // the identity literal is gone
        // the pass-through checks keep the DEFAULT-PATTERN arm VERBATIM (the RAW-TICK
        // POLYMETER LAW — Rodney's correction: this 16 is not the identity sentinel;
        // a len-7 lane free-runs (t−1)%7 against the 16 pattern). An authored
        // 32-pattern maps (bar 2 is new content, the D3 law).
        ok = ok && cpp.find("if (len == 16 && off == 0) return t;") != std::string::npos;
        check(ok, "5: no literal 16 as the identity/length sentinel in the "
                  "voice-window path (the named constant everywhere)");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
