// test_MpeRoundTrip.cpp — MpeTransmitter → MpeReceiver identity.
//
// The capstone that makes our MPE pair trustworthy without a DAW in the loop: drive the OUT core with
// Keppel-style values, pipe its emitted messages straight into the IN core, and assert the receiver
// reconstructs the same thing. Two levels of identity:
//   (1) MIDI-domain: note / bend14 / CC74 / pressure / velocity survive tx→rx EXACTLY (integer identity).
//   (2) CV-domain: CV in → forward-encode → tx → rx → inverse-decode → CV out, within quantisation.
// If both hold, the two Rack-agnostic cores are exact inverses and the module faces are the only
// unverified surface.
//
// Pure C++/no Rack; registered in run_all.sh.
//   g++ -std=c++20 -Isrc test/test_MpeRoundTrip.cpp -o /tmp/mpert && /tmp/mpert

#include <cmath>
#include <iostream>
#include <string>

#include "dsp/MpeTransmitter.hpp"
#include "dsp/MpeReceiver.hpp"

using namespace dotModular::mpe;

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)

static int g_pass = 0, g_fail = 0;

// Pipe everything the transmitter queued this block into the receiver, then clear. Handshake CCs land on
// the master channel + carry non-74 controllers, which the receiver ignores — so no filtering is needed.
static void pipe(MpeTransmitter& tx, MpeReceiver& rx) {
    for (const auto& m : tx.outbox) rx.handleMessage(m.status, m.channel, m.d1, m.d2);
    tx.outbox.clear();
}

int main() {
    const float R = 48.f;

    SUITE("MIDI-domain identity (tx → rx carries values verbatim)");
    TEST("note / bend14 / CC74 / pressure / velocity survive exactly", {
        MpeTransmitter tx; MpeReceiver rx; rx.bendRangeSemis = R;
        tx.beginBlock((int)R, 0);
        tx.noteOn(0, /*note*/67, /*bend14*/9001, /*cc74*/70, /*pressure*/55, /*vel*/113, 0);
        pipe(tx, rx);
        EXPECT(rx.active(0));
        EXPECT(rx.note(0)   == 67);
        EXPECT(rx.bend14(0) == 9001);
        EXPECT(rx.voices[0].cc74     == 70);
        EXPECT(rx.voices[0].pressure == 55);
        EXPECT(rx.voices[0].velocity == 113);
    });
    TEST("note-off carries release velocity through", {
        MpeTransmitter tx; MpeReceiver rx; rx.bendRangeSemis = R;
        tx.beginBlock((int)R, 0); tx.noteOn(0, 60, 8192, 64, 0, 100, 0); pipe(tx, rx);
        tx.beginBlock((int)R, 0); tx.noteOff(0, /*relVel*/42, 0); pipe(tx, rx);
        EXPECT(!rx.active(0) && rx.voices[0].releaseVel == 42);
    });
    TEST("continuous updates track on the receiver (bend / Y / Z)", {
        MpeTransmitter tx; MpeReceiver rx; rx.bendRangeSemis = R;
        tx.beginBlock((int)R, 0); tx.noteOn(0, 60, 8192, 64, 0, 100, 0); pipe(tx, rx);
        tx.beginBlock((int)R, 0); tx.updateContinuous(0, 7000, 90, 33, 0); pipe(tx, rx);
        EXPECT(rx.bend14(0) == 7000 && rx.voices[0].cc74 == 90 && rx.voices[0].pressure == 33);
    });
    TEST("re-articulation updates the receiver's note on the same voice", {
        MpeTransmitter tx; MpeReceiver rx; rx.bendRangeSemis = R;
        tx.beginBlock((int)R, 0); tx.noteOn(0, 60, 8192, 64, 0, 100, 0); pipe(tx, rx);
        tx.beginBlock((int)R, 0); tx.rearticulate(0, 63, 8000, 70, 10, 0); pipe(tx, rx);
        EXPECT(rx.active(0) && rx.note(0) == 63 && rx.bend14(0) == 8000);
    });

    SUITE("multi-voice independence");
    TEST("three voices round-trip to three distinct receiver voices", {
        MpeTransmitter tx; MpeReceiver rx; rx.bendRangeSemis = R;
        tx.beginBlock((int)R, 0);
        tx.noteOn(0, 60, 8192, 64, 0, 100, 0);   // → member ch 1 → rx voice 0
        tx.noteOn(1, 64, 8192, 64, 0, 100, 0);   // → member ch 2 → rx voice 1
        tx.noteOn(2, 67, 8700, 80, 20, 100, 0);  // → member ch 3 → rx voice 2
        pipe(tx, rx);
        EXPECT(rx.note(0) == 60 && rx.note(1) == 64 && rx.note(2) == 67);
        EXPECT(rx.bend14(2) == 8700 && rx.voices[2].cc74 == 80 && rx.voices[2].pressure == 20);
        EXPECT(rx.active(0) && rx.active(1) && rx.active(2) && !rx.active(3));
    });

    SUITE("CV-domain round-trip (Keppel conventions, within quantisation)");
    TEST("pitch reconstructs sub-cent across the range; Y/Z/vel recover their 7-bit value", {
        MpeTransmitter tx; MpeReceiver rx; rx.bendRangeSemis = R;
        // Sweep pitch, timbre, pressure, velocity as CV, encode exactly as Keppel does, loop, decode.
        for (float pitchV : {-2.0f, -0.5f, 0.0f, 0.371f, 1.25f, 3.0f}) {
            const int note   = noteFor(pitchV);
            const int bend   = bend14For(pitchV, R);
            const float yV   = 1.9f, zV = 4.2f, velV = 7.5f;
            const int cc74   = yCc74FromVolts(yV);
            const int press  = zPressureFromVolts(zV);
            const int vel    = (int)std::lround(velV * 12.7);
            tx.beginBlock((int)R, 0);
            tx.noteOn(0, note, bend, cc74, press, vel, 0);
            pipe(tx, rx);
            // Pitch: V/OCT reconstructs the original within sub-cent (1 cent ≈ 0.00083 V).
            EXPECT(std::fabs(rx.pitchVolts(0) - pitchV) < 0.0009f);
            // Y / Z: the 7-bit code recovered on the receiver re-encodes to the same code it was sent.
            EXPECT(yCc74FromVolts(rx.yVolts(0)) == cc74);
            EXPECT(zPressureFromVolts(rx.zVolts(0)) == press);
            // Velocity: 0..127 recovers to ~the sent volts.
            EXPECT(std::fabs(rx.velVolts(0) - velV) < 0.05f);
            tx.beginBlock((int)R, 0); tx.noteOff(0, 64, 0); pipe(tx, rx);
        }
    });

    SUITE("fixed allocation → voice-stable round-trip");
    TEST("Keppel Fixed maps voice v → rx voice v, regardless of which voices are active", {
        MpeTransmitter tx; tx.alloc = MpeTransmitter::Alloc::Fixed; MpeReceiver rx; rx.bendRangeSemis = R;
        tx.beginBlock((int)R, 0);
        tx.noteOn(2, 64, 8192, 64, 0, 100, 0);   // only voice 2 active → member ch 3 → rx voice 2
        pipe(tx, rx);
        EXPECT(rx.active(2) && rx.note(2) == 64);
        EXPECT(!rx.active(0) && !rx.active(1));   // not smeared onto voice 0 (the LRU-walk failure)
    });

    SUITE("handshake carries the bend range (auto-match)");
    TEST("rx.detectedBendRange picks up the range Keppel put in its MPE config", {
        MpeTransmitter tx; MpeReceiver rx;
        tx.beginBlock(12, 0);                     // handshake at ±12
        pipe(tx, rx);
        EXPECT(rx.detectedBendRange == 12);
        tx.beginBlock(48, 0);                     // range change → re-handshake at ±48
        pipe(tx, rx);
        EXPECT(rx.detectedBendRange == 48);
    });

    std::cout << "\n" << (g_fail ? "\033[31m" : "\033[32m")
              << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail ? 1 : 0;
}
