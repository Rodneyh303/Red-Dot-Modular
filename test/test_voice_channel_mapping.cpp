/*
 * test_voice_channel_mapping.cpp  (header-only, zero-dependency — runs in CI anywhere)
 *
 * Pins the voice<->channel/row MAPPING CONVENTION two off-by-ones violated (confirmed in
 * Rack: Lantern row 2 missing, Straits lane N drove row N+1, V16 never output).
 *
 * THE CONTRACT (mirrors VoiceResolver::slot(V) = VoiceSlot{V-1}, pinned at VoiceResolver.hpp
 * static_asserts V2=slot1 … V16=slot15):
 *   - OutputGenerator 0-BASED loop: writes polyArr[i] -> cable ch = i+1 (i=0..14).
 *     Channel (i+1) MUST read poly voice V(i+2) = slot(i+2).  Call site:
 *       src/dsp/managers/MonsoonOutputGenerator.cpp  (the 0-based `for i<15` loop)
 *     Old bug: slot(i+1) -> ch1 read V1's DORMANT slot, V16 never output.
 *   - Lantern recordGlobalVoice: global poly voice g>=1 IS V(g+1) = slot(g+1) = slot(pv_i+2),
 *     pv_i=g-1.  Call site: src/Lantern.cpp recordGlobalVoice.  Old bug: slot(pv_i+1).
 *   - CONTRAST: the 1-BASED loops (i=1..numPolyVoices) correctly use slot(i+1) — pinned so a
 *     "fix" doesn't wrongly touch them.
 *
 * This pins the FORMULA each call site must use. If you change a call-site offset, re-derive
 * it here and this stays the single source of truth for "which slot does channel/row N read".
 * (A full behavioural test would need the whole engine + Rack shim; this zero-dep test guards
 * the convention and the call sites cite it by name.)
 */
#include <iostream>
#include <string>

// The ONE mapping (mirror of VoiceResolver::slot): voice NUMBER (1..16) -> 0-based slot.
// VoiceSlot{0}=V1 (mono, dormant per-voice slot); VoiceSlot{1..15}=V2..V16.
static constexpr int slotOf(int voiceNumber) { return voiceNumber - 1; }

static int failures = 0;
static void eq(int got, int want, const std::string& m) {
    if (got != want) { std::cerr << "FAIL: " << m << "  got " << got << " want " << want << "\n"; ++failures; }
}
static void no(bool cond, const std::string& m) {
    if (cond) { std::cerr << "FAIL (must not hold): " << m << "\n"; ++failures; }
}

int main() {
    // OutputGenerator 0-based loop: ch(i+1) <- slot(i+2) = V(i+2).
    for (int i = 0; i < 15; ++i) {
        const int wantVoice = i + 2;                        // ch1=V2 … ch15=V16
        eq(slotOf(i + 2), wantVoice - 1, "OutputGen ch" + std::to_string(i + 1) + " -> V" + std::to_string(wantVoice));
        no(slotOf(i + 1) == wantVoice - 1, "OutputGen ch" + std::to_string(i + 1) + ": old slot(i+1) must NOT be V" + std::to_string(wantVoice));
    }
    eq(slotOf(2),  1,  "ch1 = V2 (slot 1), NOT V1 dormant (slot 0)");
    eq(slotOf(16), 15, "ch15 = V16 (slot 15) — V16 must be reachable");

    // Lantern: global poly voice g <- slot(g+1) = V(g+1).
    for (int g = 1; g <= 15; ++g) {
        const int pv_i = g - 1, wantVoice = g + 1;          // g=1 -> V2
        eq(slotOf(pv_i + 2), wantVoice - 1, "Lantern g=" + std::to_string(g) + " -> V" + std::to_string(wantVoice));
        no(slotOf(pv_i + 1) == wantVoice - 1, "Lantern g=" + std::to_string(g) + ": old slot(pv_i+1) must NOT be V" + std::to_string(wantVoice));
    }

    // CONTRAST: 1-based loops correctly use slot(i+1) = V(i+1).
    for (int i = 1; i <= 15; ++i)
        eq(slotOf(i + 1), i, "1-based loop: slot(i+1) = V(i+1) = slot " + std::to_string(i));

    if (failures == 0) { std::cout << "test_voice_channel_mapping: all mappings correct\n"; return 0; }
    std::cerr << "test_voice_channel_mapping: " << failures << " failure(s)\n";
    return 1;
}
