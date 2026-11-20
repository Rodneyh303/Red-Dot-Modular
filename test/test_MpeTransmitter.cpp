// test_MpeTransmitter.cpp — dotModular::mpe::MpeTransmitter, the generic CV→MPE-out substrate.
//
// Characterises the extracted Layer-1 transmitter (channel allocation, MPE config handshake, note
// lifecycle with RELEASE VELOCITY, dedupe) so the shared core is pinned independently of the Rack module
// face (src/Keppel.cpp), which cannot be compiled here. What is asserted here is what ships, since the
// module delegates to this class.
//
// Pure C++/no Rack (MpeTransmitter.hpp is stdlib-only), builds standalone. Registered in run_all.sh.
//
// Build (standalone):
//   g++ -std=c++20 -Isrc test/test_MpeTransmitter.cpp -o /tmp/mpetx && /tmp/mpetx

#include <iostream>
#include <string>
#include <vector>

#include "dsp/MpeTransmitter.hpp"

using namespace dotModular::mpe;

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)

static int g_pass = 0, g_fail = 0;

static int statusNibble(const RawMsg& m) { return m.status & 0x0f ? m.status : m.status; } // status already a nibble
static int count(const std::vector<RawMsg>& o, uint8_t status) {
    int n = 0; for (auto& m : o) if (m.status == status) ++n; return n;
}
static int indexOf(const std::vector<RawMsg>& o, uint8_t status) {
    for (int i = 0; i < (int)o.size(); ++i) if (o[i].status == status) return i;
    return -1;
}

int main() {
    SUITE("config handshake");
    TEST("fires on first block, not on a repeat at the same range, again on range change", {
        MpeTransmitter tx;
        tx.beginBlock(2, 0);
        EXPECT(!tx.outbox.empty());                 // handshake on init
        // MCM opens on the master: RPN MSB/LSB then member count on CC6.
        EXPECT(tx.outbox[0].status == 0xB && tx.outbox[0].channel == 0 && tx.outbox[0].d1 == 101);
        EXPECT(tx.outbox[1].d1 == 100 && tx.outbox[1].d2 == 0x06);
        EXPECT(tx.outbox[2].d1 == 6   && tx.outbox[2].d2 == tx.memberCount);
        size_t handshakeLen = tx.outbox.size();
        tx.beginBlock(2, 0);
        EXPECT(tx.outbox.empty());                  // same range → no re-handshake (and cleared)
        tx.beginBlock(4, 0);
        EXPECT(!tx.outbox.empty());                 // range changed → re-handshake
        EXPECT(tx.outbox.size() == handshakeLen);   // same shape, new range value
    });
    TEST("bend-range value lands on CC6 of every member + master (RPN 0)", {
        MpeTransmitter tx; tx.memberCount = 4;
        tx.beginBlock(7, 0);
        // After the 5-message MCM block, each of master+members carries a 6-message RPN-0 group whose
        // CC6 (Data Entry MSB) == the bend range.
        int sixes = 0;
        for (auto& m : tx.outbox) if (m.status == 0xB && m.d1 == 6 && m.d2 == 7) ++sixes;
        EXPECT(sixes == 5);                         // master + 4 members
    });

    SUITE("note-on");
    TEST("emits bend → note-on → CC74 → pressure on the first member channel, state latched", {
        MpeTransmitter tx; tx.beginBlock(2, 0);     // flush handshake
        tx.beginBlock(2, 0);                        // clear
        tx.noteOn(/*v*/0, /*note*/60, /*bend*/9000, /*cc74*/70, /*pressure*/20, /*vel*/100, /*f*/5);
        EXPECT(tx.outbox.size() == 4);
        EXPECT(tx.outbox[0].status == 0xE && tx.outbox[0].channel == 1);              // bend first
        EXPECT(tx.outbox[1].status == 0x9 && tx.outbox[1].d1 == 60 && tx.outbox[1].d2 == 100);
        EXPECT(tx.outbox[2].status == 0xB && tx.outbox[2].d1 == 74 && tx.outbox[2].d2 == 70);
        EXPECT(tx.outbox[3].status == 0xD && tx.outbox[3].size == 2 && tx.outbox[3].d1 == 20);
        EXPECT(tx.active(0) && tx.noteOf(0) == 60 && tx.voices[0].vel == 100);
        EXPECT(tx.voices[0].memberCh == 1);
        // 14-bit bend split: 9000 = 0x2328 → LSB 0x28, MSB 0x46.
        EXPECT(tx.outbox[0].d1 == (9000 & 0x7f) && tx.outbox[0].d2 == ((9000 >> 7) & 0x7f));
    });

    SUITE("dedupe (continuous tracking)");
    TEST("unchanged values emit nothing; only the changed dimension is re-sent", {
        MpeTransmitter tx; tx.beginBlock(2, 0); tx.beginBlock(2, 0);
        tx.noteOn(0, 60, 9000, 70, 20, 100, 0);
        tx.beginBlock(2, 0);                        // clear
        tx.updateContinuous(0, 9000, 70, 20, 1);    // identical to latched
        EXPECT(tx.outbox.empty());
        tx.updateContinuous(0, 9100, 70, 20, 1);    // bend only
        EXPECT(tx.outbox.size() == 1 && tx.outbox[0].status == 0xE);
        tx.beginBlock(2, 0);
        tx.updateContinuous(0, 9100, 80, 30, 1);    // Y + Z only (bend unchanged)
        EXPECT(tx.outbox.size() == 2);
        EXPECT(indexOf(tx.outbox, 0xB) >= 0 && indexOf(tx.outbox, 0xD) >= 0);
        EXPECT(count(tx.outbox, 0xE) == 0);
    });

    SUITE("note-off + RELEASE VELOCITY");
    TEST("note-off is 0x8 carrying the release velocity, and frees the member channel", {
        MpeTransmitter tx; tx.beginBlock(2, 0); tx.beginBlock(2, 0);
        tx.noteOn(0, 60, 8192, 64, 0, 100, 0);
        tx.beginBlock(2, 0);
        tx.noteOff(0, /*relVel*/40, 7);
        EXPECT(tx.outbox.size() == 1);
        EXPECT(tx.outbox[0].status == 0x8 && tx.outbox[0].d1 == 60 && tx.outbox[0].d2 == 40);
        EXPECT(!tx.active(0) && tx.voices[0].memberCh == -1);
        EXPECT(tx.memberOwner[1] == -1);            // channel returned to the free pool
    });
    TEST("release velocity rides through unchanged (0 and 127 extremes)", {
        MpeTransmitter tx; tx.beginBlock(2, 0); tx.beginBlock(2, 0);
        tx.noteOn(0, 48, 8192, 64, 0, 90, 0); tx.beginBlock(2, 0);
        tx.noteOff(0, 0, 1);
        EXPECT(tx.outbox[0].d2 == 0);
        tx.noteOn(1, 50, 8192, 64, 0, 90, 0); tx.beginBlock(2, 0);
        tx.noteOff(1, 127, 1);
        EXPECT(tx.outbox[0].d2 == 127);
    });

    SUITE("re-articulation");
    TEST("same member channel: note-off (neutral release) → bend → note-on (latched vel) → Y/Z", {
        MpeTransmitter tx; tx.beginBlock(2, 0); tx.beginBlock(2, 0);
        tx.noteOn(0, 60, 8192, 64, 0, 100, 0);
        int ch = tx.voices[0].memberCh;
        tx.beginBlock(2, 0);
        tx.rearticulate(0, /*newNote*/62, /*newBend*/8000, /*cc74*/70, /*pressure*/10, 9);
        EXPECT(tx.outbox.size() == 5);
        EXPECT(tx.outbox[0].status == 0x8 && tx.outbox[0].d1 == 60 && tx.outbox[0].d2 == MpeTransmitter::NEUTRAL_RELEASE);
        EXPECT(tx.outbox[1].status == 0xE);
        EXPECT(tx.outbox[2].status == 0x9 && tx.outbox[2].d1 == 62 && tx.outbox[2].d2 == 100); // reused vel
        EXPECT(tx.outbox[3].status == 0xB && tx.outbox[3].d1 == 74 && tx.outbox[3].d2 == 70);
        EXPECT(tx.outbox[4].status == 0xD && tx.outbox[4].d1 == 10);
        EXPECT(tx.voices[0].memberCh == ch && tx.noteOf(0) == 62);   // same channel, new note
    });

    SUITE("LRU allocation + steal");
    TEST("members allocate in order, then the oldest is stolen with a note-off", {
        MpeTransmitter tx; tx.memberCount = 2; tx.beginBlock(2, 0); tx.beginBlock(2, 0);
        tx.noteOn(0, 60, 8192, 64, 0, 100, 0);      // → member 1
        tx.noteOn(1, 62, 8192, 64, 0, 100, 0);      // → member 2
        EXPECT(tx.voices[0].memberCh == 1 && tx.voices[1].memberCh == 2);
        tx.beginBlock(2, 0);
        tx.noteOn(2, 64, 8192, 64, 0, 100, 0);      // none free → steal oldest (v0 on ch1)
        // First message must be the stolen note's note-off on ch1, before v2's note-on reuses ch1.
        EXPECT(tx.outbox[0].status == 0x8 && tx.outbox[0].channel == 1 && tx.outbox[0].d1 == 60);
        EXPECT(tx.outbox[0].d2 == MpeTransmitter::NEUTRAL_RELEASE);
        EXPECT(!tx.active(0));                        // v0 evicted
        EXPECT(tx.voices[2].memberCh == 1 && tx.active(2));
        EXPECT(indexOf(tx.outbox, 0x9) > 0 && tx.outbox[indexOf(tx.outbox, 0x9)].d1 == 64);
    });

    SUITE("teardown");
    TEST("allNotesOff silences every active voice and clears the pool", {
        MpeTransmitter tx; tx.memberCount = 4; tx.beginBlock(2, 0); tx.beginBlock(2, 0);
        tx.noteOn(0, 60, 8192, 64, 0, 100, 0);
        tx.noteOn(1, 62, 8192, 64, 0, 100, 0);
        tx.beginBlock(2, 0);
        tx.allNotesOff(0);
        EXPECT(count(tx.outbox, 0x8) == 2);
        EXPECT(!tx.active(0) && !tx.active(1));
        EXPECT(tx.lruOrder.empty());
        for (int i = 0; i < 16; ++i) EXPECT(tx.memberOwner[i] == -1);
    });

    std::cout << "\n" << (g_fail ? "\033[31m" : "\033[32m")
              << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    (void)statusNibble;
    return g_fail ? 1 : 0;
}
