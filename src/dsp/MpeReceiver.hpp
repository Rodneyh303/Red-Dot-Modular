#pragma once
// ── dotModular::mpe::MpeReceiver — the generic MPE-in → poly state substrate (no Rack) ───────────────
// The mirror of MpeTransmitter: where the transmitter turns poly CV into MPE messages, the receiver
// consumes MPE messages and maintains the per-voice state a module reads back as poly CV. It is the
// inverse direction and the EASIER half — the hard MPE problems (channel allocation, voice stealing, the
// config handshake) all live on the OUT side. Here the sender already assigned member channels, so this
// is bookkeeping: track each member channel's note/bend/CC74/pressure/velocity and map member channel →
// output poly voice.
//
// Rack-agnostic + unit-testable: the host pops midi::Message from its InputQueue and calls
// handleMessage(status, channel, d1, d2); each block it reads the per-voice accessors (pitchVolts /
// yVolts / zVolts / velVolts / gate ...) and writes its poly outputs. No Rack, no byte parsing here.
// stdlib-only (test/test_MpeReceiver.cpp). Shares MpeMath for the value↔CV conversions, so an
// out→in loop through Keppel is identity.
//
// MPE Lower Zone: master = channel index 0 (MIDI ch 1); members = indices 1..memberCount (MIDI ch 2..).
// Member channel index `ch` maps to output poly voice `ch-1`. v1 handles member-channel (per-note)
// messages — the per-note bend is what microtonal reconstruction needs; zone-wide master-channel
// handling (global bend applied to all voices) is a documented refinement, not done here.

#include <array>
#include <cstdint>

#include "dsp/MpeMath.hpp"

namespace dotModular {
namespace mpe {

struct MpeReceiver {
    static constexpr int MAX_MEMBERS = 15;
    int   memberCount    = MAX_MEMBERS;   // lower-zone size; voices 0..memberCount-1 are usable
    float bendRangeSemis = 48.f;          // MUST match the sender's per-note bend range (MPE default ±48)

    struct Voice {
        bool active     = false;   // gate (note currently held)
        int  note       = 60;      // last note-on number (held through the note)
        int  bend14     = 8192;    // member-channel pitch bend, 14-bit centred at 8192
        int  velocity   = 0;       // last note-on velocity (0..127)
        int  releaseVel  = 64;     // last note-off velocity (0..127; neutral 64 until a release arrives)
        int  cc74       = 64;      // Y / timbre (0..127, rest 64)
        int  pressure   = 0;       // Z / channel pressure (0..127, rest 0)
    };
    std::array<Voice, 16> voices{};

    void reset() { for (auto& v : voices) v = Voice{}; }

    // Member channel index (1..memberCount) → output poly voice (0..memberCount-1); -1 if out of zone
    // (master channel 0, or beyond the lower zone).
    int voiceForChannel(int ch) const {
        return (ch >= 1 && ch <= memberCount) ? ch - 1 : -1;
    }

    // Consume one MIDI message. status is the high nibble (0x8/0x9/0xB/0xD/0xE); channel is 0..15.
    void handleMessage(uint8_t status, uint8_t channel, uint8_t d1, uint8_t d2) {
        int v = voiceForChannel(channel);
        if (v < 0) return;                         // master / out-of-zone: v1 ignores (see header)
        Voice& vs = voices[v];
        switch (status & 0xF0) {
            case 0x90:                             // note-on (velocity 0 == note-off)
                if (d2 > 0) { vs.active = true; vs.note = d1; vs.velocity = d2; }
                else        { vs.active = false; vs.releaseVel = 64; }
                break;
            case 0x80:                             // note-off (+ release velocity)
                vs.active = false; vs.releaseVel = d2;
                break;
            case 0xE0:                             // pitch bend (LSB d1, MSB d2) → member bend
                vs.bend14 = (int)d1 | ((int)d2 << 7);
                break;
            case 0xB0:                             // CC — MPE Y is CC74
                if (d1 == 74) vs.cc74 = d2;
                break;
            case 0xD0:                             // channel pressure → Z
                vs.pressure = d1;
                break;
            default: break;                        // poly aftertouch (0xA) etc.: v1 ignores
        }
    }

    // ── per-voice readouts (poly CV), in the SAME conventions Keppel emits ───────────────────────────
    bool  active(int v)      const { return voices[v].active; }
    int   note(int v)        const { return voices[v].note; }
    int   bend14(int v)      const { return voices[v].bend14; }
    // V/OCT with the member bend folded in at bendRangeSemis — the single "true (microtonal) pitch" out
    // stock MIDI-CV makes you reconstruct by hand.
    float pitchVolts(int v)  const { return reconstructVolts(voices[v].note, voices[v].bend14, bendRangeSemis); }
    float xBendSemis(int v)  const { return bendSemisFrom14(voices[v].bend14, bendRangeSemis); }   // standalone X (bend) out
    float yVolts(int v)      const { return yVoltsFromCc74(voices[v].cc74); }        // CC74 timbre → bipolar around 0V
    float zVolts(int v)      const { return zVoltsFromPressure(voices[v].pressure); }// channel pressure → 0..10V
    float velVolts(int v)    const { return velVoltsFrom127(voices[v].velocity); }   // note-on velocity → 0..10V
    float relVelVolts(int v) const { return velVoltsFrom127(voices[v].releaseVel); } // release velocity → 0..10V
    float gateVolts(int v)   const { return voices[v].active ? 10.f : 0.f; }

    int   maxVoices()        const { return memberCount; }
};

}  // namespace mpe
}  // namespace dotModular
