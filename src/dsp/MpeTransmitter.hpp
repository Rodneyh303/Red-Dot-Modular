#pragma once
// ── dotModular::mpe::MpeTransmitter — the generic CV→MPE-out transmitter substrate (no Rack) ─────────
// Layer 1 of the MPE_UTILITY_BUILD_SPEC separation-of-concerns: member-channel allocation (LRU), the MPE
// lower-zone configuration handshake, the per-voice note lifecycle (note-on / note-off with RELEASE
// VELOCITY / forced re-articulation), continuous bend/CC74/pressure with dedupe, and the exact wire
// encoding. It knows NOTHING about tuning, CV voltages, or dot.modular — it deals only in MIDI-domain
// quantities (note number, 14-bit bend, 7-bit CC74/pressure, 7-bit velocities, member channel). ANY poly
// CV source can drive it; the microtonal split, the A/B expression window, and the opinionated accent
// defaults all live in the module FACE that feeds this class.
//
// Transport-agnostic + unit-testable: every message is appended to `outbox` as a RawMsg; the host drains
// the outbox into its midi::Output each block (converting RawMsg -> midi::Message). Tests inspect the
// outbox directly. stdlib-only, builds standalone (test/test_MpeTransmitter.cpp).
//
// MPE Lower Zone: master = MIDI channel index 0 (ch 1); members = indices 1..memberCount (ch 2..). We
// never emit note data on the master; members carry the per-note bend/CC/pressure.

#include <array>
#include <cstdint>
#include <vector>

namespace dotModular {
namespace mpe {

// A raw MIDI message in MIDI-domain terms (no Rack). `size` is 2 (channel pressure) or 3 (everything
// else). status is the high nibble (0x8 note-off, 0x9 note-on, 0xB CC, 0xD channel pressure, 0xE bend).
struct RawMsg {
    uint8_t status;
    uint8_t channel;   // 0..15
    uint8_t d1;
    uint8_t d2;
    uint8_t size;      // 2 or 3
    int64_t frame;
};

struct MpeTransmitter {
    static constexpr int MAX_MEMBERS = 15;
    static constexpr int NEUTRAL_RELEASE = 64;   // internal note-offs (steal / re-articulate) carry a
                                                 // neutral release velocity — no player release occurred.
    int memberCount = MAX_MEMBERS;

    // Per-voice (poly channel) assignment + last-sent state, for edge-driven lifecycle + dedupe.
    struct Voice {
        bool active     = false;
        int  memberCh   = -1;    // 1..memberCount, or -1
        int  note       = 60;    // latched for the note's lifetime
        int  vel        = 80;    // latched note-on velocity (reused on re-articulation)
        int  lastBend14 = 8192;
        int  lastY74    = 64;
        int  lastZ7     = 0;
    };
    std::array<Voice, 16> voices{};

    std::array<int, 16> memberOwner;   // index 1..memberCount: owning voice, or -1 free (0 = master, unused)
    std::vector<int> lruOrder;    // member indices, oldest first (front stolen first)
    bool needHandshake = true;
    int  lastBendRange = -1;

    std::vector<RawMsg> outbox;

    MpeTransmitter() { for (int i = 0; i < 16; ++i) memberOwner[i] = -1; outbox.reserve(256); }

    // ── wire encoding ─────────────────────────────────────────────────────────────────────────────
    void emit3(uint8_t st, uint8_t ch, uint8_t d1, uint8_t d2, int64_t f) {
        outbox.push_back(RawMsg{st, ch, d1, d2, 3, f});
    }
    void cc(uint8_t ch, uint8_t c, uint8_t val, int64_t f)            { emit3(0xB, ch, c, (uint8_t)(val & 0x7f), f); }
    void bendMsg(uint8_t ch, int pw14, int64_t f)                    { emit3(0xE, ch, (uint8_t)(pw14 & 0x7f), (uint8_t)((pw14 >> 7) & 0x7f), f); }
    void noteOnMsg(uint8_t ch, uint8_t note, uint8_t vel, int64_t f) { emit3(0x9, ch, note, vel, f); }
    void noteOffMsg(uint8_t ch, uint8_t note, uint8_t relVel, int64_t f) { emit3(0x8, ch, note, (uint8_t)(relVel & 0x7f), f); }
    // Channel pressure is a 2-byte message (0xDn, value) — build it explicitly so no stray 3rd byte
    // reaches picky MPE receivers.
    void pressureMsg(uint8_t ch, uint8_t val, int64_t f) {
        outbox.push_back(RawMsg{0xD, ch, (uint8_t)(val & 0x7f), 0, 2, f});
    }

    // ── MPE config handshake: lower-zone member count (MCM, RPN 6) + per-note bend range (RPN 0) on
    // master + every member. Sent on init / member-count change / bend-range change. ─────────────────
    void configMpe(int bendRangeSemis, int64_t f) {
        cc(0, 101, 0x00, f);                      // RPN MSB
        cc(0, 100, 0x06, f);                      // RPN LSB = 6 (MCM)
        cc(0,   6, (uint8_t)memberCount, f);      // Data Entry MSB = member channel count
        cc(0, 101, 0x7f, f);                      // RPN Null
        cc(0, 100, 0x7f, f);
        for (int ch = 0; ch <= memberCount; ++ch) {
            cc((uint8_t)ch, 101, 0x00, f);
            cc((uint8_t)ch, 100, 0x00, f);        // RPN LSB = 0 (pitch-bend sensitivity)
            cc((uint8_t)ch,   6, (uint8_t)bendRangeSemis, f);
            cc((uint8_t)ch,  38, 0x00, f);        // cents = 0
            cc((uint8_t)ch, 101, 0x7f, f);
            cc((uint8_t)ch, 100, 0x7f, f);
        }
    }

    // Call once per block before the per-voice updates: clears the outbox and (re)handshakes on init or
    // when the bend range changes. Returns nothing; drain outbox after the voice loop.
    void beginBlock(int bendRangeSemis, int64_t f) {
        outbox.clear();
        if (needHandshake || bendRangeSemis != lastBendRange) {
            configMpe(bendRangeSemis, f);
            lastBendRange = bendRangeSemis;
            needHandshake = false;
        }
    }

    // ── member-channel allocation (LRU) ─────────────────────────────────────────────────────────────
    int allocMember(int v, int64_t f) {
        for (int ch = 1; ch <= memberCount; ++ch) {
            if (memberOwner[ch] < 0) { memberOwner[ch] = v; lruOrder.push_back(ch); return ch; }
        }
        // None free → steal the oldest: note-off its owner (neutral release) first.
        if (!lruOrder.empty()) {
            int ch = lruOrder.front(); lruOrder.erase(lruOrder.begin());
            int prev = memberOwner[ch];
            if (prev >= 0 && voices[prev].active) {
                noteOffMsg((uint8_t)ch, (uint8_t)voices[prev].note, NEUTRAL_RELEASE, f);
                voices[prev].active = false;
                voices[prev].memberCh = -1;
            }
            memberOwner[ch] = v; lruOrder.push_back(ch); return ch;
        }
        return -1;
    }
    void freeMember(int ch) {
        if (ch < 0) return;
        memberOwner[ch] = -1;
        for (auto it = lruOrder.begin(); it != lruOrder.end(); ++it)
            if (*it == ch) { lruOrder.erase(it); break; }
    }
    void allNotesOff(int64_t f) {
        for (int v = 0; v < 16; ++v) {
            if (voices[v].active && voices[v].memberCh >= 0)
                noteOffMsg((uint8_t)voices[v].memberCh, (uint8_t)voices[v].note, NEUTRAL_RELEASE, f);
            voices[v].active = false; voices[v].memberCh = -1;
        }
        for (int i = 0; i < 16; ++i) memberOwner[i] = -1;
        lruOrder.clear();
    }

    // ── per-voice note lifecycle (MIDI-domain — the module supplies the computed values) ─────────────
    // Note-on: bend FIRST (note starts at pitch, not sliding in), THEN note-on, THEN initial Y/Z so the
    // receiver opens the note with the right timbre + pressure.
    void noteOn(int v, int note, int bend14, int cc74, int pressure7, int vel, int64_t f) {
        int ch = allocMember(v, f);
        if (ch < 0) return;
        bendMsg((uint8_t)ch, bend14, f);
        noteOnMsg((uint8_t)ch, (uint8_t)note, (uint8_t)vel, f);
        cc((uint8_t)ch, 74, (uint8_t)cc74, f);
        pressureMsg((uint8_t)ch, (uint8_t)pressure7, f);
        Voice& vs = voices[v];
        vs.active = true; vs.memberCh = ch; vs.note = note; vs.vel = vel;
        vs.lastBend14 = bend14; vs.lastY74 = cc74; vs.lastZ7 = pressure7;
    }
    // Note-off with RELEASE VELOCITY (0x8 + velocity byte). Frees the member channel.
    void noteOff(int v, int relVel, int64_t f) {
        Voice& vs = voices[v];
        if (vs.memberCh >= 0) {
            noteOffMsg((uint8_t)vs.memberCh, (uint8_t)vs.note, (uint8_t)relVel, f);
            freeMember(vs.memberCh);
        }
        vs.active = false; vs.memberCh = -1;
    }
    // Forced re-articulation on the SAME member channel (over-range or within-legato step boundary):
    // note-off (neutral release — not a player release) → bend → note-on (reuse latched vel) → re-affirm
    // Y/Z at current values (carried across, no discontinuity).
    void rearticulate(int v, int newNote, int newBend14, int cc74, int pressure7, int64_t f) {
        Voice& vs = voices[v];
        if (vs.memberCh < 0) return;
        noteOffMsg((uint8_t)vs.memberCh, (uint8_t)vs.note, NEUTRAL_RELEASE, f);
        bendMsg((uint8_t)vs.memberCh, newBend14, f);
        noteOnMsg((uint8_t)vs.memberCh, (uint8_t)newNote, (uint8_t)vs.vel, f);
        cc((uint8_t)vs.memberCh, 74, (uint8_t)cc74, f);
        pressureMsg((uint8_t)vs.memberCh, (uint8_t)pressure7, f);
        vs.note = newNote; vs.lastBend14 = newBend14; vs.lastY74 = cc74; vs.lastZ7 = pressure7;
    }
    // Continuous tracking within range: note stays latched, only bend/Y/Z move. Dedupe each so a held
    // voice doesn't flood the wire at audio rate.
    void updateContinuous(int v, int bend14, int cc74, int pressure7, int64_t f) {
        Voice& vs = voices[v];
        if (vs.memberCh < 0) return;
        if (bend14    != vs.lastBend14) { bendMsg((uint8_t)vs.memberCh, bend14, f);        vs.lastBend14 = bend14; }
        if (cc74      != vs.lastY74)    { cc((uint8_t)vs.memberCh, 74, (uint8_t)cc74, f);  vs.lastY74 = cc74; }
        if (pressure7 != vs.lastZ7)     { pressureMsg((uint8_t)vs.memberCh, (uint8_t)pressure7, f); vs.lastZ7 = pressure7; }
    }

    // Queries for the module's MONITOR / lights.
    bool active(int v)     const { return voices[v].active; }
    int  noteOf(int v)     const { return voices[v].note; }
    int  lastBend14(int v) const { return voices[v].lastBend14; }
};

}  // namespace mpe
}  // namespace dotModular
