// Per-lane direction — replica of the CURRENT engine model (4-state LaneDir + poly tier).
//
// NOTE: this file previously modelled an EARLIER design (2-state fwd/rev, pendulum flipping
// at the PHRASE WRAP). The engine has since moved to a 4-state LaneDir where Pendulum and
// PingPong bounce at the **LOR WINDOW ENDPOINT** (pos len-1 fwd / pos 0 rev), and the poly
// per-voice sign is **ABSOLUTE** (no longer multiplied by mono's). The old tests still passed
// because they tested their own stale model — so they are re-pinned here to the real thing.
//
// Mirrors SequencerEngine::advancePlayhead, in order:
//   1. StepEdge : promote pending sign+dir NOW (before advancing)
//   2. sync sign from dir: Forward -> +1, Reverse -> -1; Pendulum/PingPong keep their
//      bounce-managed sign (except 0 -> +1, since -0 == 0 would never flip)
//   3. advance the lane's own tick by globalDir * sign, with:
//        PingPong : at the endpoint, HOLD one step (the endpoint repeats), then bounce
//        Pendulum : advance, then flip immediately at the endpoint (no repeat)
//   4. Phrase   : promote pending sign+dir at the window wrap (a no-op for bouncing lanes,
//                 because the bounce keeps pending in sync)
//
// Build: g++ -std=c++17 test/test_lane_direction.cpp
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) ++passed; else { ++failed; std::printf("  FAIL: %s\n", what); }
}

static const long DNA_LCM = 720720;                 // matches SequencerEngine
static long wrapT(long t) { return ((t % DNA_LCM) + DNA_LCM) % DNA_LCM; }
static int  pmod(long a, int m) { return (int)(((a % m) + m) % m); }

// Exact copy of SequencerEngine::getStrandIdx.
static int getStrandIdx(long tick, int len, int off, int rot) {
    int safeLen = std::max(1, len);
    int timelineIdx = (int)((((tick + rot) % safeLen) + safeLen) % safeLen);
    return (timelineIdx + off) % 16;
}

enum class Dir : int { Forward = 0, Reverse = 1, Pendulum = 2, PingPong = 3 };
enum class Quant { StepEdge, Phrase };

struct Lane {
    long tick = 0;
    int  sign = +1, signPending = +1;
    Dir  dir = Dir::Forward, dirPending = Dir::Forward;
    bool pingHold = false;
    int  len = 16;                 // this strand's LOR length (the bounce window)

    void syncSign() {
        if (dir == Dir::Forward)      sign = +1;
        else if (dir == Dir::Reverse) sign = -1;
        else if (sign == 0)           sign = +1;   // -0 == 0 would never flip
    }
    void advance(int globalDir) {
        if (dir == Dir::PingPong && pingHold) {          // bounce after the repeat
            pingHold = false; sign = -sign; signPending = sign;
        } else if (dir == Dir::PingPong) {
            int pos = pmod(tick, len);
            if ((sign > 0 && pos == len - 1) || (sign < 0 && pos == 0)) {
                pingHold = true; return;                 // HOLD: the endpoint repeats
            }
        }
        tick = wrapT(tick + (long)globalDir * sign);
        if (dir == Dir::Pendulum) {                      // flip at the endpoint, no repeat
            int pos = pmod(tick, len);
            if ((sign > 0 && pos == len - 1) || (sign < 0 && pos == 0)) {
                sign = -sign; signPending = sign;
            }
        }
    }
    void step(int globalDir, Quant q, bool wrapped) {
        if (q == Quant::StepEdge) { sign = signPending; dir = dirPending; }
        syncSign();
        advance(globalDir);
        if (q == Quant::Phrase && wrapped) { sign = signPending; dir = dirPending; }
    }
    void setDir(Dir d) { dirPending = d; signPending = (d == Dir::Reverse) ? -1 : +1; }
};

// Collect the window positions a lane produces, starting from its current tick.
static std::vector<int> walk(Lane& ln, int n, Quant q = Quant::StepEdge) {
    std::vector<int> out;
    out.push_back(pmod(ln.tick, ln.len));            // starting position
    for (int i = 0; i < n; ++i) { ln.step(+1, q, false); out.push_back(pmod(ln.tick, ln.len)); }
    return out;
}

int main() {
    std::printf("\033[1mPer-lane direction (current 4-state engine model)\033[0m\n");
    std::printf("%s\n", std::string(50, '=').c_str());

    const int L = 7, O = 5, R = 3;   // an arbitrary window for the cell-level tests

    // ── 1. Forward lane is bit-identical to the global tick (no regression) ──────
    {
        Lane ln; ln.len = L;
        long total = 0; bool same = true;
        for (int s = 0; s < 200; ++s) {
            ln.step(+1, Quant::StepEdge, false);
            total = wrapT(total + 1);
            if (ln.tick != total) same = false;
            if (getStrandIdx(ln.tick, L,O,R) != getStrandIdx(total, L,O,R)) same = false;
        }
        check(same, "Forward lane == global tick (bit-identical; no-op until reversed)");
    }

    // ── 2. Reverse retraces the cells just played (adjacent turnaround, no jump) ─
    {
        Lane ln; ln.len = L;
        std::vector<int> fwd;
        for (int s = 0; s < 5; ++s) { ln.step(+1, Quant::StepEdge, false); fwd.push_back(getStrandIdx(ln.tick,L,O,R)); }
        ln.setDir(Dir::Reverse);
        std::vector<int> rev;
        for (int s = 0; s < 5; ++s) { ln.step(+1, Quant::StepEdge, false); rev.push_back(getStrandIdx(ln.tick,L,O,R)); }
        check(rev[0]==fwd[3] && rev[1]==fwd[2] && rev[2]==fwd[1] && rev[3]==fwd[0],
              "Reverse retraces the cells just played (no mirror jump)");
    }

    // ── 3. Pendulum bounces at the LOR WINDOW ENDPOINT — no endpoint repeat ─────
    {
        Lane ln; ln.len = 4; ln.dir = Dir::Pendulum; ln.dirPending = Dir::Pendulum;
        // from pos 0: 1,2,3(flip),2,1,0(flip),1,2,3 — endpoints visited ONCE each
        check(walk(ln, 9) == std::vector<int>({0,1,2,3,2,1,0,1,2,3}),
              "Pendulum bounces at the LOR endpoint, no endpoint repeat");
    }

    // ── 4. PingPong bounces at the endpoint AND repeats it ─────────────────────
    {
        Lane ln; ln.len = 4; ln.dir = Dir::PingPong; ln.dirPending = Dir::PingPong;
        // from pos 0: 1,2,3,3(hold=repeat),2,1,0,0(repeat),1 — endpoints play TWICE
        check(walk(ln, 9) == std::vector<int>({0,1,2,3,3,2,1,0,0,1}),
              "PingPong repeats the endpoint step, then bounces");
    }

    // ── 5. The bounce is keyed to the LANE'S OWN len, not to the phrase ─────────
    // The earlier model flipped at the phrase wrap. A len=4 lane must bounce every 4 steps
    // even under Phrase quant with no wrap ever reported.
    {
        Lane ln; ln.len = 4; ln.dir = Dir::Pendulum; ln.dirPending = Dir::Pendulum;
        std::vector<int> pos; pos.push_back(pmod(ln.tick, ln.len));
        for (int s = 0; s < 9; ++s) {
            ln.step(+1, Quant::Phrase, /*wrapped=*/false);
            pos.push_back(pmod(ln.tick, ln.len));
        }
        check(pos == std::vector<int>({0,1,2,3,2,1,0,1,2,3}),
              "Pendulum bounces on its own LOR window under Phrase quant (not at the wrap)");
    }

    // ── 6. Manual flip quantization: StepEdge immediate, Phrase deferred ────────
    {
        Lane a; a.len = 16; a.setDir(Dir::Reverse);
        long before = a.tick;
        a.step(+1, Quant::StepEdge, false);
        check(a.tick == wrapT(before - 1), "StepEdge: manual reverse takes effect immediately");

        Lane b; b.len = 16; b.setDir(Dir::Reverse);
        long p = b.tick;
        for (int s = 0; s < 3; ++s) { b.step(+1, Quant::Phrase, false);
            check(b.tick == p + 1, "Phrase: mid-phrase stays forward (flip deferred)"); p = b.tick; }
        b.step(+1, Quant::Phrase, /*wrapped=*/true);        // promotes at the wrap
        p = b.tick;
        b.step(+1, Quant::Phrase, false);
        check(b.tick == wrapT(p - 1), "Phrase: reverse takes effect on the step after the wrap");
    }

    // ── 7. Composition with global Mode E reverse ──────────────────────────────
    {
        Lane a; a.len = 16;                                  // Forward lane
        a.step(-1, Quant::StepEdge, false);
        check(a.tick == wrapT(-1), "Forward lane tracks Mode E reverse (tick--)");
        Lane b; b.len = 16; b.setDir(Dir::Reverse); b.dir = Dir::Reverse; b.sign = -1;
        b.step(-1, Quant::StepEdge, false);
        check(b.tick == +1, "Reverse lane under Mode E goes forward (double negative)");
    }

    // ── 8. Poly per-voice sign is ABSOLUTE, not relative to mono ────────────────
    // The engine changed from effSign = laneSign_[l] * polyLaneSign(v,l) to just
    // polyLaneSign(v,l): a Forward voice stays FORWARD even when mono's lane is Reversed
    // (it does NOT inherit mono's reversal); a Reverse voice reverses regardless of mono.
    {
        Lane mono; mono.len = 16; mono.setDir(Dir::Reverse); mono.dir = Dir::Reverse; mono.sign = -1;
        Lane voiceFwd; voiceFwd.len = 16;                    // Forward (default)
        Lane voiceRev; voiceRev.len = 16; voiceRev.setDir(Dir::Reverse); voiceRev.dir = Dir::Reverse; voiceRev.sign = -1;
        mono.step(+1, Quant::StepEdge, false);
        voiceFwd.step(+1, Quant::StepEdge, false);
        voiceRev.step(+1, Quant::StepEdge, false);
        check(mono.tick == wrapT(-1),     "mono lane Reverse walks backward");
        check(voiceFwd.tick == +1,        "poly Forward voice stays FORWARD under a reversed mono lane");
        check(voiceRev.tick == wrapT(-1), "poly Reverse voice walks backward");
        check(voiceFwd.tick != mono.tick, "=> poly direction is ABSOLUTE, not mono-relative");
    }

    // ── 9. A bouncing lane keeps `pending` in sync, so Phrase promotion can't clobber it ──
    {
        Lane ln; ln.len = 4; ln.dir = Dir::Pendulum; ln.dirPending = Dir::Pendulum;
        for (int s = 0; s < 3; ++s) ln.step(+1, Quant::Phrase, false);   // reaches pos 3 → flips to -1
        check(ln.sign == -1 && ln.signPending == -1, "bounce keeps signPending in sync");
        ln.step(+1, Quant::Phrase, /*wrapped=*/true);                    // promotion must be a no-op
        check(ln.sign == -1, "Phrase promotion does not clobber a bounce-managed sign");
    }

    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
