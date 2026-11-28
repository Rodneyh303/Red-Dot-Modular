// test_cycle_phrase_mode.cpp — CC_PROMPT_UAS_CYCLE_PHRASE_MODE (engine wiring).
// Phrase mode locks the voice window to the transport: O=0, R=transport offset, L=transport
// length — the authored L/O/R are ignored (direction stays free). Custom mode (the ENGINE
// default) reads the authored window verbatim, so every existing test/golden is unchanged. At
// the default transport (cap) Phrase resolves to the old (cap,0,0), so a fresh module is
// byte-identical until a global length/offset is authored.
#include "test_stubs.hpp"
#include "engines/SequencerEngine.hpp"
#include <iostream>

static int s_pass = 0, s_fail = 0;
#define TEST(desc, ...) do { \
    bool _ok = true; std::string _m; \
    try { __VA_ARGS__; } catch (const std::exception& _e) { _ok = false; _m = _e.what(); } \
    if (_ok) { ++s_pass; std::cout << "  \033[32m✓\033[0m " << (desc) << "\n"; } \
    else { ++s_fail; std::cout << "  \033[31m✗\033[0m " << (desc); if(!_m.empty()) std::cout << " — " << _m; std::cout << "\n"; } \
} while (0)
#define EXPECT(e) do { if (!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while (0)

using E = SequencerEngine;

int main() {
    std::cout << "\033[1mCycle: Phrase mode (engine wiring)\033[0m\n" << std::string(50, '=') << "\n";

    // T1 — Custom (the engine default): the authored voice window is read verbatim.
    TEST("Custom (default): the authored Macro-row window is read verbatim", {
        E e; e.reset();
        e.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);     // transport len 16
        e.macroVoiceLor_[E::LOR_LEN] = 24; e.macroVoiceLor_[E::LOR_OFF] = 3; e.macroVoiceLor_[E::LOR_ROT] = 5;
        int L, O, R; E::LaneDir d; e.effectiveMacroRowLor(L, O, R, d);
        EXPECT(L == 24 && O == 3 && R == 5);
        EXPECT(!e.macroCyclePhrase());                          // default is Custom
    });

    // T2 — Phrase: the window follows the transport (L=len, O=0, R=offset); authored IGNORED.
    TEST("Phrase: Macro-row window follows the transport (16,0,0), authored (24,3,5) ignored", {
        E e; e.reset();
        e.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        e.macroVoiceLor_[E::LOR_LEN] = 24; e.macroVoiceLor_[E::LOR_OFF] = 3; e.macroVoiceLor_[E::LOR_ROT] = 5;
        e.setMacroCyclePhrase(true);
        int L, O, R; E::LaneDir d; e.effectiveMacroRowLor(L, O, R, d);
        EXPECT(L == 16 && O == 0 && R == 0);
    });

    // T3 — Phrase: the voice ROTATION follows the transport OFFSET (Rodney's rule).
    TEST("Phrase: R follows the transport offset (offset 5 -> R=5)", {
        E e; e.reset();
        e.updateWindow(16.f, 0.f, false, 5.f, 0.f, false);     // transport offset 5
        e.setMacroCyclePhrase(true);
        int L, O, R; E::LaneDir d; e.effectiveMacroRowLor(L, O, R, d);
        EXPECT(L == 16 && O == 0 && R == 5);
    });

    // T4 — default-config equivalence: Phrase at the default transport (cap) == the old default
    //      authored window (cap,0,0). A fresh module is byte-identical.
    TEST("Phrase at the default transport (cap) == old default window (cap,0,0)", {
        E eP; eP.reset(); eP.setMacroCyclePhrase(true);
        E eC; eC.reset();                                       // Custom default, authored = (cap,0,0)
        int L1, O1, R1, L2, O2, R2; E::LaneDir d;
        eP.effectiveMacroRowLor(L1, O1, R1, d);
        eC.effectiveMacroRowLor(L2, O2, R2, d);
        EXPECT(L1 == L2 && O1 == O2 && R1 == R2);
    });

    // T5 — a VOICE row follows the Macro's mode via delegation (localEast=false).
    TEST("delegated voice: inherits the Macro row's Phrase mode", {
        E e; e.reset();
        e.updateWindow(12.f, 0.f, false, 2.f, 0.f, false);     // transport len 12, offset 2
        e.macroVoiceLor_[E::LOR_LEN] = 24; e.macroVoiceLor_[E::LOR_OFF] = 6; e.macroVoiceLor_[E::LOR_ROT] = 7;
        e.setMacroCyclePhrase(true);                            // voice 0 delegates -> inherits Phrase
        int L, O, R; E::LaneDir d; e.effectiveVoiceLor(0, L, O, R, d);
        EXPECT(L == 12 && O == 0 && R == 2);                    // follows transport, not authored (24,6,7)
    });

    std::cout << std::string(50, '=') << "\n"
              << (s_fail == 0 ? "\033[32m" : "\033[31m")
              << s_pass << " passed, " << s_fail << " failed\033[0m\n";
    return s_fail == 0 ? 0 : 1;
}
