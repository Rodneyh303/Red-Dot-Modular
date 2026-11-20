// test_MpeReceiver.cpp — dotModular::mpe::MpeReceiver (MPE-in → poly state) + the MpeMath inverse mappers.
//
// The mirror of test_MpeTransmitter: characterises the MPE-in substrate (channel→voice mapping, note
// lifecycle with release velocity, bend/CC74/pressure state) and proves the value↔CV conversions are
// exact inverses, so an out→in loop through Keppel is identity. Pure C++/no Rack; registered in run_all.sh.
//
// Build (standalone):
//   g++ -std=c++20 -Isrc test/test_MpeReceiver.cpp -o /tmp/mperx && /tmp/mperx

#include <cmath>
#include <iostream>
#include <string>

#include "dsp/MpeReceiver.hpp"

using namespace dotModular::mpe;

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)

static int g_pass = 0, g_fail = 0;

int main() {
    SUITE("MpeMath inverses round-trip exactly");
    TEST("yCc74FromVolts ∘ yVoltsFromCc74 == identity for every 0..127", {
        for (int cc = 0; cc <= 127; ++cc)
            EXPECT(yCc74FromVolts(yVoltsFromCc74(cc)) == cc);
    });
    TEST("zPressureFromVolts ∘ zVoltsFromPressure == identity for every 0..127", {
        for (int p = 0; p <= 127; ++p)
            EXPECT(zPressureFromVolts(zVoltsFromPressure(p)) == p);
    });
    TEST("Y rest 64 → 0V; Z rest 0 → 0V; velocity 127 → ~10V", {
        EXPECT(std::fabs(yVoltsFromCc74(64)) < 1e-6);
        EXPECT(std::fabs(zVoltsFromPressure(0)) < 1e-6);
        EXPECT(std::fabs(velVoltsFrom127(127) - 10.f) < 0.05f);
    });
    TEST("bendSemisFrom14 is the inverse of bend14 (centre + a few offsets, ±48)", {
        EXPECT(std::fabs(bendSemisFrom14(8192, 48.f)) < 1e-6);               // centre → 0
        for (float s : {-30.f, -1.f, 2.5f, 40.f}) {
            int b = bend14(s, 48.f);
            EXPECT(std::fabs(bendSemisFrom14(b, 48.f) - s) < 0.02f);         // sub-cent at ±48
        }
    });

    SUITE("note lifecycle");
    TEST("note-on sets gate/note/velocity; note-off clears gate and latches release velocity", {
        MpeReceiver rx;
        rx.handleMessage(0x90, 1, 60, 100);        // ch1 (member) → voice 0
        EXPECT(rx.active(0) && rx.note(0) == 60 && rx.voices[0].velocity == 100);
        EXPECT(rx.gateVolts(0) == 10.f);
        EXPECT(std::fabs(rx.velVolts(0) - 100.f / 12.7f) < 1e-4);
        rx.handleMessage(0x80, 1, 60, 40);         // note-off, release vel 40
        EXPECT(!rx.active(0) && rx.gateVolts(0) == 0.f);
        EXPECT(rx.voices[0].releaseVel == 40);
        EXPECT(std::fabs(rx.relVelVolts(0) - 40.f / 12.7f) < 1e-4);
    });
    TEST("note-on with velocity 0 is a note-off", {
        MpeReceiver rx;
        rx.handleMessage(0x90, 2, 64, 90);
        EXPECT(rx.active(1));
        rx.handleMessage(0x90, 2, 64, 0);          // vel 0 == note-off
        EXPECT(!rx.active(1));
    });

    SUITE("channel → voice mapping");
    TEST("member channel ch maps to voice ch-1; master (0) and out-of-zone are ignored", {
        MpeReceiver rx; rx.memberCount = 4;
        rx.handleMessage(0x90, 1, 60, 80);
        rx.handleMessage(0x90, 4, 72, 80);
        EXPECT(rx.active(0) && rx.active(3) && rx.note(3) == 72);
        rx.handleMessage(0x90, 0, 48, 80);         // master channel → ignored
        rx.handleMessage(0x90, 5, 48, 80);         // beyond lower zone (memberCount 4) → ignored
        EXPECT(!rx.active(4));
        EXPECT(rx.voiceForChannel(0) == -1 && rx.voiceForChannel(1) == 0 && rx.voiceForChannel(4) == 3);
        EXPECT(rx.voiceForChannel(5) == -1);
    });

    SUITE("expression dimensions");
    TEST("pitch bend folds into V/OCT via reconstructVolts at the set range", {
        MpeReceiver rx; rx.bendRangeSemis = 48.f;
        rx.handleMessage(0x90, 1, 60, 100);        // note 60 → 0V base
        EXPECT(std::fabs(rx.pitchVolts(0)) < 1e-6);
        rx.handleMessage(0xE0, 1, 0x00, 0x60);     // bend MSB 0x60 (LSB 0) = 12288
        int expect14 = 0x00 | (0x60 << 7);
        EXPECT(rx.bend14(0) == expect14);
        EXPECT(std::fabs(rx.pitchVolts(0) - reconstructVolts(60, expect14, 48.f)) < 1e-6);
        // +half-range bend (12288 vs centre 8192, half of 8192) ≈ +24 st = +2V at ±48.
        EXPECT(std::fabs(rx.pitchVolts(0) - 2.f) < 0.02f);
    });
    TEST("CC74 → Y volts; channel pressure → Z volts (inverse of the forward mappers)", {
        MpeReceiver rx;
        rx.handleMessage(0x90, 1, 60, 100);
        rx.handleMessage(0xB0, 1, 74, 96);         // CC74 = 96
        EXPECT(rx.voices[0].cc74 == 96);
        EXPECT(std::fabs(rx.yVolts(0) - yVoltsFromCc74(96)) < 1e-6);
        rx.handleMessage(0xB0, 1, 1, 100);         // CC1 (mod wheel) is NOT Y — ignored
        EXPECT(rx.voices[0].cc74 == 96);
        rx.handleMessage(0xD0, 1, 80, 0);          // channel pressure = 80
        EXPECT(rx.voices[0].pressure == 80);
        EXPECT(std::fabs(rx.zVolts(0) - zVoltsFromPressure(80)) < 1e-6);
    });

    SUITE("out→in loop identity (Keppel conventions)");
    TEST("CC74/pressure survive a value→CV→value round-trip through both headers", {
        MpeReceiver rx;
        rx.handleMessage(0x90, 1, 60, 100);
        for (int cc : {0, 32, 64, 96, 127}) {
            rx.handleMessage(0xB0, 1, 74, (uint8_t)cc);
            EXPECT(yCc74FromVolts(rx.yVolts(0)) == cc);        // in → CV → (what Keppel would re-encode)
        }
        for (int p : {0, 25, 64, 127}) {
            rx.handleMessage(0xD0, 1, (uint8_t)p, 0);
            EXPECT(zPressureFromVolts(rx.zVolts(0)) == p);
        }
    });

    std::cout << "\n" << (g_fail ? "\033[31m" : "\033[32m")
              << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail ? 1 : 0;
}
