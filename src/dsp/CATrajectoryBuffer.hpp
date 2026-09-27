// CATrajectoryBuffer.hpp — TRUE REVERSE trajectory-replay engine (CA_DICE_COUNTER_MODEL.md).
//
// A per-stream ring of COMMITTED pin states (src[16] for rhythm/melody/q-mix), pushed once per
// phrase-boundary commit AFTER the verbs apply (records RESULTS, not causes — that is what makes
// true-reverse verb-agnostic and able to step back through a lossy collapse, the reason trajectory
// replay was chosen over transform inversion). On consume (a queued true-reverse request at the
// phrase boundary), stepBack() pops the newest entry and restores the PREVIOUS committed state.
//
// Rack-free / header-only so the engine is unit-testable without the SDK (mirrors
// ChangeAlleyTransforms.hpp). The MonsoonChangeAlleyV2 module composes this struct.
//
// DESIGN (per the doc):
//   - Fixed capacity, allocated once at construction — NO audio-thread allocation.
//   - Depth: 65536 phrases/stream ("effectively unlimited"; 65536 x 16 B x 3 = ~3 MB, well under
//     the 10 MB ceiling). One entry per phrase boundary (states change once per phrase).
//   - pushIfChanged: skip when the stream's state didn't change this commit, so the buffer spans
//     far more musical time and isn't consumed by static passages.
//   - End of buffer: STOP (stepBack returns false), don't wrap — wrapping would silently replay
//     the WRONG history.
//   - Serialise a bounded TAIL only (256 newest/stream, ~12 KB), newest-first, with a format
//     version + stream-count + state-size guard so a stale/mismatched blob is DROPPED rather than
//     restoring garbage pins. Restore into the head; reversing past the restored tail stops cleanly.
//     Do NOT serialise the whole ring — Rack patches are JSON and autosave periodically.
//
// Ring model: base = physical slot of the OLDEST entry; count = number of live entries (0..DEPTH).
// Slot of the i-th logical entry (oldest=0) is (base + i) % DEPTH. Newest = (base+count-1)%DEPTH.
// push appends at (base+count)%DEPTH, sliding base when full (oldest falls off). stepBack pops the
// newest (count--); a later push overwrites the popped slot = branching (the reversed future is
// discarded, exactly like undo-then-redo-branch).
//
// Thread model: the module calls pushIfChanged/stepBack on the audio thread (control-rate, at the
// phrase boundary). Single-threaded — the SPSC hand-off is the existing undoRing; this buffer is a
// single-thread audio-thread performance buffer (distinct lifetime, distinct consumer).
#pragma once
#include <cstdint>
#include <cstring>   // memcmp, memcpy

namespace redDot {

struct CATrajectoryBuffer {
    static constexpr int N_VOICES = 16;
    static constexpr int N_STREAMS = 3;          // rhythm / melody / q-mix
    static constexpr int DEPTH = 65536;          // phrases per stream (~3 MB total; "effectively unlimited")
    static constexpr int SERIAL_TAIL = 256;      // bounded tail persisted to JSON (~12 KB)
    static constexpr uint32_t SERIAL_VERSION = 1;// layout guard; bump on any change

    struct State { uint8_t src[N_VOICES]; };

    State ring[N_STREAMS][DEPTH];
    int   base[N_STREAMS]  = {0,0,0};   // physical slot of the oldest live entry
    int   count[N_STREAMS] = {0,0,0};   // number of live entries (0..DEPTH)

    CATrajectoryBuffer() = default;

    void clear() {
        for (int s = 0; s < N_STREAMS; ++s) { base[s] = 0; count[s] = 0; }
    }

    int size(int s) const { return (s >= 0 && s < N_STREAMS) ? count[s] : 0; }

    // Push a stream's committed state IF it differs from the newest live state. Called once per
    // phrase-boundary commit, AFTER verbs apply. Skipping unchanged states keeps the buffer
    // spanning more musical time AND makes each stepBack land on a genuinely different state.
    void pushIfChanged(int stream, const uint8_t src[N_VOICES]) {
        if (stream < 0 || stream >= N_STREAMS) return;
        const int n = count[stream];
        if (n > 0) {
            const State& newest = ring[stream][(base[stream] + n - 1) % DEPTH];
            if (std::memcmp(newest.src, src, N_VOICES) == 0) return;   // unchanged → skip
        }
        if (n >= DEPTH) {
            // Full: overwrite the oldest (slide the window). The newest stays the newest.
            std::memcpy(ring[stream][base[stream]].src, src, N_VOICES);
            base[stream] = (base[stream] + 1) % DEPTH;   // oldest falls off, window slides
            // count stays at DEPTH.
        } else {
            std::memcpy(ring[stream][(base[stream] + n) % DEPTH].src, src, N_VOICES);
            ++count[stream];
        }
    }

    // Step one entry BACKWARD for `stream`: pop the newest live entry and write the PREVIOUS
    // committed state into out[]. Returns true if a state was restored; returns false at the
    // buffer start (0 or 1 entries → nothing to reverse to → STOP, don't wrap).
    bool stepBack(int stream, uint8_t out[N_VOICES]) {
        if (stream < 0 || stream >= N_STREAMS) return false;
        if (count[stream] <= 1) return false;   // nothing before the single current state → stop
        --count[stream];                        // pop the newest
        const int prevSlot = (base[stream] + count[stream] - 1) % DEPTH;
        std::memcpy(out, ring[stream][prevSlot].src, N_VOICES);
        return true;
    }

    // ── Serialisation: bounded TAIL, newest-first, with a version guard ──────────────────────
    // Rack-free: we expose the raw blob; the module owns the json_t plumbing.
    struct SerialBlob {
        uint32_t version;
        int      nStreams;
        int      stateSize;
        int      counts[N_STREAMS];
        State    states[N_STREAMS][SERIAL_TAIL];
    };

    SerialBlob serialise() const {
        SerialBlob b{};
        b.version = SERIAL_VERSION;
        b.nStreams = N_STREAMS;
        b.stateSize = (int)N_VOICES;
        for (int s = 0; s < N_STREAMS; ++s) {
            int n = count[s];
            if (n > SERIAL_TAIL) n = SERIAL_TAIL;
            b.counts[s] = n;
            // newest-first: newest is at (base+count-1), walk back.
            for (int i = 0; i < n; ++i) {
                int idx = (base[s] + count[s] - 1 - i + DEPTH) % DEPTH;
                b.states[s][i] = ring[s][idx];
            }
        }
        return b;
    }

    // Restore a SerialBlob. A stale/mismatched blob (wrong version / nStreams / stateSize, or a
    // count out of range) is REJECTED (returns false, leaves the buffer untouched). The tail is
    // restored oldest-first so stepBack walks it in the right order; reversing past the restored
    // tail hits the stop condition cleanly.
    bool deserialise(const SerialBlob& b) {
        if (b.version != SERIAL_VERSION) return false;
        if (b.nStreams != N_STREAMS)      return false;
        if (b.stateSize != (int)N_VOICES) return false;
        for (int s = 0; s < N_STREAMS; ++s)
            if (b.counts[s] < 0 || b.counts[s] > SERIAL_TAIL) return false;   // corrupt → reject
        clear();
        for (int s = 0; s < N_STREAMS; ++s) {
            const int n = b.counts[s];
            // blob is newest-first (index 0 = newest); push oldest-first to rebuild live order.
            for (int i = n - 1; i >= 0; --i) {
                std::memcpy(ring[s][(base[s] + count[s]) % DEPTH].src, b.states[s][i].src, N_VOICES);
                ++count[s];
            }
        }
        return true;
    }
};

} // namespace redDot
