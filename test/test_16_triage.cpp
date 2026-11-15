// test_16_triage.cpp — CC_PROMPT_16_TRIAGE (option C, Rodney's ruling) + the cycle-mode
// phrase anchor (Prompt 2). "16" means THREE different things in the engine; each
// occurrence is triaged, never blanket-replaced:
//   bucket 1 — the POLYMETER PASS-THROUGH: the 16 arms stay VERBATIM (the raw-tick
//              law; the grep pin in test_macro_shared_state gate 5 pins the exact
//              string) and each of the four law sites (driverTickAt, driverTickBefore,
//              the two passG twins) GAINS the cap arm || (len == kPatternLengthCap &&
//              off == 0) — the full-ring window passes through, congruent for every
//              reader (a non-divisive lane free-runs against the 32 exactly as it did
//              against the 16).
//   bucket 2 — the IDENTITY/TRANSPORT defaults: the out-of-range fallbacks and the
//              TRANSPORT default (cachedLength/endStep/globalWalk_) take the cap
//              (read-identical under the cap arm); the LANE-WINDOW LOR seeds stay 16,
//              NAMED as kDefaultPatternLen (a distinct constant) so the default-config
//              goldens hold — moving them to the cap is the DEFERRED round.
//   bucket 3 — the VOICE-COUNT 16: named as kVoiceSlots (hoisted above VoiceBank;
//              the in-class alias keeps SequencerEngine::kVoiceSlots resolving), value
//              unchanged. The D10/D15 display-lamp 16s are named kRingLamps.
// Prompt 2 — the CYCLE phrase anchor: the frame walk folds the PHRASE-RELATIVE
//   position (wGv — the transport's position within the pattern, 0 at global step 1),
//   not the free-running tick, so R re-anchors at EVERY global step 1 (the UAS nesting
//   reference: R is the voice's position when the transport hits step 1). East and
//   Macro share the path (the voice rows + the macro row); the abs1 branch is
//   unchanged (the full phrase, transport-synced by design).

#include <cstdio>
#include <cctype>
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

// Comment-stripped source (keeps newlines so the failure messages carry line numbers).
static std::string stripComments(const std::string& src) {
    std::string out; out.reserve(src.size());
    bool lineC = false, blockC = false;
    for (size_t i = 0; i < src.size(); ++i) {
        const char c = src[i];
        const char n = (i + 1 < src.size()) ? src[i + 1] : '\0';
        if (!lineC && !blockC && c == '/' && n == '/') { lineC = true; out += "  "; ++i; continue; }
        if (!lineC && !blockC && c == '/' && n == '*') { blockC = true; out += "  "; ++i; continue; }
        if (lineC && c == '\n') { lineC = false; out += '\n'; continue; }
        if (blockC && c == '*' && n == '/') { blockC = false; out += "  "; ++i; continue; }
        if (lineC || blockC) out += (c == '\n') ? '\n' : ' ';
        else                 out += c;
    }
    return out;
}

// A standalone decimal token ("16" at true WORD boundaries — not part of a longer
// number and not inside an identifier like pulsesPer16th).
static bool wordChar(char c) { return std::isalnum((unsigned char)c) || c == '_'; }
static bool hasStandalone(const std::string& line, const char* tok) {
    const std::string t = tok;
    size_t p = 0;
    while ((p = line.find(t, p)) != std::string::npos) {
        const bool lo = (p == 0) || !wordChar(line[p - 1]);
        const bool hi = (p + t.size() >= line.size()) || !wordChar(line[p + t.size()]);
        if (lo && hi) return true;
        p += t.size();
    }
    return false;
}

// The ALLOWLIST — the only sanctioned literal 16s in the engine's code:
static bool allowed16(const std::string& line) {
    return line.find("len == 16 && off == 0") != std::string::npos      // bucket 1: the polymeter law's 16 arms (driverTickAt/Before — VERBATIM, the grep pin)
        || line.find("lenG == 16 && offG == 0") != std::string::npos    // bucket 1: the passG twins (the same law's representative test)
        || line.find("kVoiceSlots = 16") != std::string::npos           // bucket 3: the voice-count definition (the named constant's own value)
        || line.find("kDefaultPatternLen = 16") != std::string::npos    // bucket 2: the default-pattern-length definition (the stay-16 group)
        || (line.find("kRingLamps") != std::string::npos
            && line.find("= 16") != std::string::npos);                 // the D10/D15 display-lamp constant's definition
}

int main() {
    const int CAP = dotModular::kPatternLengthCap;

    // ── GATE 1: THE GREP PIN — no anonymous 16/15 remains in the engine's code ──
    {
        std::string cpp, hpp;
        bool ok = slurpFile("src/dsp/engines/SequencerEngine.cpp", cpp)
               && slurpFile("src/dsp/engines/SequencerEngine.hpp", hpp);
        check(ok, "GATE 1 prereq: the engine sources slurp");
        const std::string scpp = stripComments(cpp);
        const std::string shpp = stripComments(hpp);
        bool bad16 = false, bad15 = false;
        std::string offending;
        size_t ln = 1;
        size_t start = 0;
        for (size_t i = 0; i <= scpp.size(); ++i) {
            if (i == scpp.size() || scpp[i] == '\n') {
                const std::string line = scpp.substr(start, i - start);
                if (hasStandalone(line, "16") && !allowed16(line) && !bad16) {
                    bad16 = true; offending = "SequencerEngine.cpp:" + std::to_string(ln) + ": " + line;
                }
                if (hasStandalone(line, "15") && !bad15) {
                    bad15 = true; if (offending.empty()) offending = "SequencerEngine.cpp:" + std::to_string(ln) + ": " + line;
                }
                start = i + 1; ln++;
            }
        }
        ln = 1; start = 0;
        for (size_t i = 0; i <= shpp.size(); ++i) {
            if (i == shpp.size() || shpp[i] == '\n') {
                const std::string line = shpp.substr(start, i - start);
                if (hasStandalone(line, "16") && !allowed16(line) && !bad16) {
                    bad16 = true; offending = "SequencerEngine.hpp:" + std::to_string(ln) + ": " + line;
                }
                if (hasStandalone(line, "15") && !bad15) {
                    bad15 = true; if (offending.empty()) offending = "SequencerEngine.hpp:" + std::to_string(ln) + ": " + line;
                }
                start = i + 1; ln++;
            }
        }
        check(!bad16 && !bad15, "GATE 1: no anonymous 16/15 in the engine's code — every one is kPatternLengthCap, kVoiceSlots, kDefaultPatternLen, kRingLamps or the commented bucket-1 polymeter arm");
        if ((bad16 || bad15) && !offending.empty())
            std::printf("  first offender: %s\n", offending.c_str());
    }

    // ── GATE 2: the pass-through arms (bucket 1) ──
    {
        SequencerEngine eng; eng.reset();
        check(eng.driverTickAt(37, 16, 0) == 37
           && eng.driverTickAt(37, CAP, 0) == 37
           && eng.driverTickAt(37, 10, 2) == 9,
              "GATE 2: the 16 arm verbatim + the CAP arm (the full-ring window passes through) + the mapped window walk");
    }

    // ── GATE 3: the reset state (option C's split) ──
    {
        SequencerEngine eng; eng.reset();
        const auto M = VoiceResolver::mono();
        check(eng.cachedLength == CAP && eng.endStep == CAP - 1
           && eng.globalWalk_ == dotModular::kMaskRing - 1
           && eng.dnaLength == CAP,
              "GATE 3a: the TRANSPORT default = the cap (cachedLength/endStep/globalWalk_/dnaLength)");
        check(eng.strandLen(0) == 16 && eng.macroLOR_[0] == 16,
              "GATE 3b: the LANE-WINDOW seeds = kDefaultPatternLen (16 — the stay-16 group)");
        check(eng.voiceLor(M, 0) == CAP && eng.macroVoiceLor_[0] == CAP
           && eng.macroVoiceLor_[1] == 0 && eng.macroVoiceLor_[2] == 0,
              "GATE 3c: the identity rows = the cap (bug 23 — unchanged)");
    }

    // ── GATE 4: the default march — the classic-16 lane reads + the len-7 free-run ──
    {
        SequencerEngine eng; eng.reset();
        eng.lorRef(0, SequencerEngine::LOR_LEN) = 7;   // the MEL lane authored len 7
        bool ok = true;
        for (int t = 0; t < 36; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int T = t + 1;
            ok = ok && eng.laneTick_[0] == (T - 1) % 7                     // the free-run (the cap arm)
                 && eng.laneTick_[1] == (T - 1) % 16                       // the kDefaultPatternLen lane
                 && eng.stepIndex == (T - 1) % CAP;                        // the playhead walks the full ring
        }
        check(ok, "GATE 4: at the cap default — a len-7 lane free-runs (T-1)%7, a default lane reads (T-1)%16, the playhead wraps at the cap");
    }

    // ── GATE 5: the authored 16/0 bit-identity (the 16 arm, unchanged) ──
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        eng.lorRef(0, SequencerEngine::LOR_LEN) = 7;
        bool ok = true;
        for (int t = 0; t < 21; ++t) {
            eng.advancePlayhead(1, nullptr);
            ok = ok && eng.laneTick_[0] == (t) % 7;
        }
        check(ok, "GATE 5: an authored 16/0 window reads (T-1)%7 — bit-identical to the cap default's reads (the two arms are congruent)");
    }

    // ── GATE 6: the mapped fold (unchanged) ──
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(10.f, 0.f, false, 2.f, 0.f, false);
        eng.lorRef(0, SequencerEngine::LOR_LEN) = 7;
        bool ok = true;
        for (int t = 0; t < 21; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int T = t + 1;
            ok = ok && eng.laneTick_[0] == (((T - 1) % 10) + 2) % 7;
        }
        check(ok, "GATE 6: the mapped 10@2 fold (((T-1)%10 + 2) mod 7) — unchanged");
    }

    // ── GATE 7: PROMPT 2 — the phrase anchor (the voice row + the macro twin) ──
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 7;
        eng.macroVoiceLor_[SequencerEngine::LOR_ROT] = 3;
        const auto M = VoiceResolver::mono();
        bool okV = true, okM = true, anchorOK = true;
        for (int t = 0; t < 33; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int T = t + 1;
            const int want = (((T - 1) % 16) + 3) % 7;   // the phrase-anchored fold
            okV = okV && eng.voiceReadPosition(M) == want;
            okM = okM && eng.macroFrameRead() == want;
            if (T % 16 == 1) anchorOK = anchorOK && eng.voiceReadPosition(M) == 3;   // R at EVERY global step 1
        }
        check(okV && okM,
              "GATE 7: an L=7 R=3 voice row under a 16-step pattern reads the phrase-anchored fold (((T-1)%16 + 3) mod 7) — the macro row's twin identical");
        check(anchorOK,
              "GATE 7b: the read is R (=3) at EVERY global step 1 (T=1,17,33) — the cycle restarts from R each phrase");
    }

    // ── GATE 8: dividing-L congruence (the goldens' class — unchanged) ──
    {
        SequencerEngine eng; eng.reset();
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        eng.macroVoiceLor_[SequencerEngine::LOR_LEN] = 8;
        eng.macroVoiceLor_[SequencerEngine::LOR_ROT] = 3;
        const auto M = VoiceResolver::mono();
        bool ok = true;
        for (int t = 0; t < 33; ++t) {
            eng.advancePlayhead(1, nullptr);
            const int T = t + 1;
            ok = ok && eng.voiceReadPosition(M) == (T - 1 + 3) % 8          // the old free-run law
                 && (T - 1 + 3) % 8 == (((T - 1) % 16) + 3) % 8;            // congruent: 8 | 16
        }
        check(ok, "GATE 8: a dividing row (L=8, R=3) under 16 — the phrase anchor is congruent with the old law (the goldens hold)");
    }

    // ── GATE 9: the voice count (bucket 3) — named, value unchanged ──
    {
        VoiceBank vb;
        check(SequencerEngine::kVoiceSlots == 16 && kVoiceSlots == 16
           && (int)vb.slots.size() == 16,
              "GATE 9: kVoiceSlots == 16 everywhere it is spelled (the class alias, the namespace hoist, VoiceBank's slots) — the voice count, not a step bound");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
