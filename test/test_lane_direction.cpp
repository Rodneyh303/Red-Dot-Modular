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

static const long DNA_LCM = 1441440;                // matches SequencerEngine (LCM(1..16)*2)
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


    // ── 10. STATELESS model: same sequences as the accumulator, but NO drift ─────
    // Closed-form position at global tick t (replica of SequencerEngine::laneTickFor).
    auto tickFor = [](Dir d, long t, int len) -> long {
        const int L = std::max(1, len);
        switch (d) {
            case Dir::Reverse: return -t;
            case Dir::Pendulum: {
                if (L < 2) return 0;
                const long P = 2*(L-1);
                const long u = ((t + (L-1)) % P + P) % P;
                return std::labs(u - (L-1));
            }
            case Dir::PingPong: {
                const long P = 2*L;
                const long u = ((t % P) + P) % P;
                return (u < L) ? u : (P - 1 - u);
            }
            default: return t;
        }
    };
    // (a) it reproduces the accumulator's sequences exactly — so behaviour is preserved.
    {
        const int L = 4;
        for (Dir d : {Dir::Forward, Dir::Reverse, Dir::Pendulum, Dir::PingPong}) {
            Lane acc; acc.len = L; acc.dir = d; acc.dirPending = d;
            if (d == Dir::Reverse) acc.sign = -1;
            bool same = true;
            for (int t = 0; t <= 9; ++t) {
                int want = pmod(acc.tick, L);
                int got  = pmod(tickFor(d, t, L), L);
                if (want != got) same = false;
                acc.step(+1, Quant::StepEdge, false);
            }
            check(same, "stateless closed form == accumulator sequence");
        }
    }
    // (b) THE POINT: the accumulator drifts permanently after a temporary reverse; the
    //     stateless model cannot, because position never depends on history.
    {
        const int L = 8;
        Lane a; a.len = L;                       // never flipped
        Lane b; b.len = L;                       // reversed for 3 steps, then forward again
        for (int t = 0; t < 12; ++t) {
            if (t == 2) { b.setDir(Dir::Reverse); }
            if (t == 5) { b.setDir(Dir::Forward); }
            a.step(+1, Quant::StepEdge, false);
            b.step(+1, Quant::StepEdge, false);
        }
        check(pmod(a.tick, L) != pmod(b.tick, L),
              "accumulator: a temporary reverse leaves the lane PERMANENTLY out of phase");
        // Stateless: both lanes are Forward at t=12, so both are simply f(12) — identical,
        // regardless of what either did in between.
        check(pmod(tickFor(Dir::Forward, 12, L), L) == pmod(tickFor(Dir::Forward, 12, L), L),
              "stateless: lanes back on the same dir are back in phase, whatever they did");
        // And a stateless lane always agrees with the DNA clock when Forward.
        bool locked = true;
        for (int t = 0; t < 50; ++t) if (pmod(tickFor(Dir::Forward, t, L), L) != pmod(t, L)) locked = false;
        check(locked, "stateless Forward lane is always exactly the global tick (no private offset)");
    }


    // ── 11. HYBRID: positions stateless, trajectories walked ────────────────────
    // Forward/Reverse are POSITIONS (closed form); Pendulum/PingPong are TRAJECTORIES (walker).
    // (a) LOR MODULATION — the objection that killed full stateless. `len` is CV-modulated at
    //     control rate. A closed-form bouncer's period is 2(len-1)/2*len, so its phase
    //     recomputes and the lane TELEPORTS on every modulation step; a walker just turns
    //     around at the new endpoint.
    {
        const int L1 = 4, L2 = 7;
        // Closed-form pendulum: same t, len nudged 4 -> 7. Phase is recomputed => jump.
        long a = tickFor(Dir::Pendulum, 5, L1);
        long b = tickFor(Dir::Pendulum, 5, L2);
        check(a != b, "closed-form bouncer: changing len at fixed t moves the lane (teleports)");
        // Walker: len change cannot move it — position is where it walked to; only the
        // BOUNCE POINT moves. Step it with len=4, then again with len=7, and it advances by
        // exactly one step either way.
        Lane w; w.len = L1; w.dir = Dir::Pendulum; w.dirPending = Dir::Pendulum;
        w.step(+1, Quant::StepEdge, false);
        long before = w.tick;
        w.len = L2;                                  // modulate the window mid-walk
        w.step(+1, Quant::StepEdge, false);
        check(std::labs(w.tick - before) == 1, "walker bouncer: len modulation never teleports it");
    }
    // (b) The closed-form modes are unaffected by len — the formula never mentions it, so
    //     modulation cannot perturb the no-drift guarantee.
    {
        bool same = true;
        for (int L = 1; L <= 16; ++L)
            if (tickFor(Dir::Forward, 37, L) != 37 || tickFor(Dir::Reverse, 37, L) != -37) same = false;
        check(same, "Forward/Reverse tick is independent of len (LOR modulation cannot drift them)");
    }
    // (c) RESET: zeroing the master clock syncs the closed-form lanes for free, but a walker
    //     carries on — which is why handleRestart must clear the walk state (it did not, so
    //     RESET silently stopped syncing polymeters once lanes got their own tick).
    {
        check(tickFor(Dir::Forward, 0, 8) == 0 && tickFor(Dir::Reverse, 0, 8) == 0,
              "RESET: closed-form lanes land on Beat 1 from t=0 automatically");
        Lane w; w.len = 8; w.dir = Dir::Pendulum; w.dirPending = Dir::Pendulum;
        for (int i = 0; i < 5; ++i) w.step(+1, Quant::StepEdge, false);
        check(w.tick != 0, "RESET: a walker does NOT return to Beat 1 on its own => must be cleared");
    }


    // ── 12. DNA_LCM must be a multiple of EVERY lane period ─────────────────────
    // The wrap is inaudible only if, at t=DNA_LCM, every lane is exactly where it was at t=0.
    // That needs DNA_LCM % period == 0 for each mode's period — not merely % len:
    //   Forward/Reverse  len         Pendulum  2*(len-1)        PingPong  2*len
    // LCM(1..16)=720720 carries only 2^4 and so failed the single cell len-16 PingPong (2*16=32),
    // by exactly half a period — that lane flipped direction once per wrap (~25 h at 120 BPM).
    // Guard it here so adding a mode or widening the length range can't silently reopen it.
    {
        bool ok = true;
        for (int L = 1; L <= 16; ++L) {
            if (DNA_LCM % L) ok = false;                                   // Forward / Reverse
            if (L > 1 && DNA_LCM % (2*(L-1))) ok = false;                  // Pendulum
            if (DNA_LCM % (2*L)) ok = false;                               // PingPong
        }
        check(ok, "DNA_LCM is a multiple of every lane period (fwd/rev, pendulum, pingpong)");
        check(720720 % 32 != 0, "...and LCM(1..16) alone was NOT — len-16 PingPong was the gap");
    }

    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
