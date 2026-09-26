/**
 * test_ca_true_reverse.cpp — TRUE REVERSE trajectory-replay engine.
 * Header-lite (no Rack SDK): exercises CATrajectoryBuffer directly.
 *
 * NOTE: CATrajectoryBuffer is ~3 MB (65536 phrases x 16 B x 3 streams). The module holds it as
 * a heap MEMBER (fine). Here every instance is declared `static` so it lives in BSS, NOT on the
 * stack — a 3 MB automatic would overflow the default 1 MB Windows stack at function entry
 * (exit 127). Static storage zero-initialises count[]/base[] → each starts empty (no clear needed).
 *
 * Compile (see test/run_all.sh for the canonical line):
 *   g++ -std=c++17 -Itest -Isrc/dsp test/test_ca_true_reverse.cpp -o /tmp/t_catr && /tmp/t_catr
 */
#include "../src/dsp/CATrajectoryBuffer.hpp"
#include "../src/dsp/ChangeAlleyTransforms.hpp"   // ca::collapse (lossy) for the reverse-through-collapse case
#include <cstdio>
#include <cstring>

static int pass = 0, fail = 0;
#define CHK(c, m) do { if (c) ++pass; else { ++fail; std::printf("  FAIL: %s\n", m); } } while (0)

using redDot::CATrajectoryBuffer;
using dotModular::ca::collapse;

static void ident(uint8_t s[16]) { for (int v = 0; v < 16; ++v) s[v] = (uint8_t)v; }
static bool eq(const uint8_t a[16], const uint8_t b[16]) { return std::memcmp(a, b, 16) == 0; }

int main() {
    // ── 1. Reverse replays recorded states in reverse order, including through a LOSSY collapse ─
    // The whole point of trajectory-replay over transform-inversion: collapse is lossy/non-
    // invertible, but replaying the recorded state walks back through it exactly.
    {
        static CATrajectoryBuffer buf;   // BSS, zero-init → empty
        const int stream = 0;   // rhythm
        uint8_t s0[16]; ident(s0);                                  buf.pushIfChanged(stream, s0);
        uint8_t s1[16]; ident(s1); collapse(s1, 16, 4);            buf.pushIfChanged(stream, s1);
        uint8_t s2[16]; std::memcpy(s2, s1, 16);
        dotModular::ca::rotateValues(s2, 16, 4, 1);                buf.pushIfChanged(stream, s2);

        CHK(buf.size(stream) == 3, "three distinct states recorded");

        uint8_t out[16];
        bool ok1 = buf.stepBack(stream, out);
        CHK(ok1 && eq(out, s1), "stepBack #1 -> collapsed state s1 (through the lossy collapse)");
        bool ok2 = buf.stepBack(stream, out);
        CHK(ok2 && eq(out, s0), "stepBack #2 -> identity s0");
        bool ok3 = buf.stepBack(stream, out);
        CHK(!ok3, "stepBack #3 -> STOP at buffer start (don't wrap)");
        CHK(buf.size(stream) == 1, "size decremented to 1 after two reverses");
    }

    // ── 2. End-of-buffer: STOP, don't wrap (single state, and empty) ─────────────────────────
    {
        static CATrajectoryBuffer buf;
        uint8_t out[16], s[16]; ident(s);
        buf.pushIfChanged(1, s);
        CHK(buf.size(1) == 1, "one state on melody stream");
        CHK(!buf.stepBack(1, out), "single state -> stop (nothing to reverse to)");
        CHK(!buf.stepBack(2, out), "empty q-mix stream -> stop");
    }

    // ── 3. pushIfChanged skips unchanged states (depth spans more musical time) ──────────────
    {
        static CATrajectoryBuffer buf;
        uint8_t s[16]; ident(s);
        buf.pushIfChanged(0, s);
        buf.pushIfChanged(0, s);   // identical -> skipped
        buf.pushIfChanged(0, s);   // identical -> skipped
        CHK(buf.size(0) == 1, "identical pushes collapsed to one entry");
        uint8_t c[16]; ident(c); collapse(c, 16, 4);
        buf.pushIfChanged(0, c);
        CHK(buf.size(0) == 2, "changed state recorded -> size 2");
    }

    // ── 4. Stale / mismatched serialised blob is REJECTED, not restored as garbage ──────────
    {
        static CATrajectoryBuffer buf;
        uint8_t s[16]; ident(s); buf.pushIfChanged(0, s);
        uint8_t c[16]; ident(c); collapse(c, 16, 4); buf.pushIfChanged(0, c);
        auto good = buf.serialise();

        static CATrajectoryBuffer restored;
        CHK(restored.deserialise(good), "well-formed blob accepted");
        CHK(restored.size(0) == 2, "restored count matches");

        auto bad = good; bad.version = 999;
        static CATrajectoryBuffer r2; CHK(!r2.deserialise(bad), "wrong version rejected");
        CHK(r2.size(0) == 0, "rejected blob left buffer empty");

        auto bad2 = good; bad2.nStreams = 99;
        CHK(!restored.deserialise(bad2), "wrong nStreams rejected");

        auto bad3 = good; bad3.stateSize = 7;
        CHK(!restored.deserialise(bad3), "wrong stateSize rejected");

        auto bad4 = good; bad4.counts[0] = 99999;
        CHK(!restored.deserialise(bad4), "out-of-range count rejected");
    }

    // ── 5. Restored tail reverses correctly, then stops past the restored window ────────────
    {
        static CATrajectoryBuffer buf;
        uint8_t a[16]; ident(a);              buf.pushIfChanged(0, a);
        uint8_t b2[16]; ident(b2); collapse(b2, 16, 4); buf.pushIfChanged(0, b2);
        uint8_t c2[16]; std::memcpy(c2, b2, 16); dotModular::ca::rotateValues(c2, 16, 4, 1); buf.pushIfChanged(0, c2);
        auto blob = buf.serialise();

        static CATrajectoryBuffer r;
        CHK(r.deserialise(blob), "tail restored");
        uint8_t out[16];
        CHK(r.stepBack(0, out) && eq(out, b2), "restored: stepBack #1 -> middle state");
        CHK(r.stepBack(0, out) && eq(out, a),  "restored: stepBack #2 -> first state");
        CHK(!r.stepBack(0, out), "restored: stepBack #3 -> stop past restored tail (no wrap)");
    }

    // ── 6. Branching: reverse then push forward discards the reversed future ────────────────
    {
        static CATrajectoryBuffer buf;
        uint8_t a[16]; ident(a);              buf.pushIfChanged(0, a);
        uint8_t b2[16]; ident(b2); collapse(b2, 16, 4); buf.pushIfChanged(0, b2);
        uint8_t c2[16]; std::memcpy(c2, b2, 16); dotModular::ca::rotateValues(c2, 16, 4, 1); buf.pushIfChanged(0, c2);
        uint8_t out[16];
        buf.stepBack(0, out);                 // now at b2
        CHK(eq(out, b2), "reverse to b2 before branching");
        uint8_t d[16]; ident(d); collapse(d, 16, 8); buf.pushIfChanged(0, d);  // branch
        bool ok = buf.stepBack(0, out);
        CHK(ok && eq(out, b2), "after branch, stepBack -> branched-from state (old future discarded)");
    }

    std::printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
