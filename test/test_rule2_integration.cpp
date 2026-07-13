// EAST_EXTRA_LANES Stage 3 — Rule 2 ROLL→CONSUME integrated across a mono legato run.
//
// test_poly_slur_roll pins the roll and test_rule2_consume pins the consume in isolation.
// This runs the two together through a mono chain the way executePolyVoice does — roll at
// the onset, consume + re-roll at each landing — to prove the per-voice legato actually
// diverges end to end, and to pin the ONE case where it CANNOT: LegatoMax.
//
// Model matches executePolyVoice's state machine. The only per-voice input is the legato
// draw r at each step (mono's cell when delegated, the voice's own cell when Local East);
// prob is mono's global legatoProb; notes are whole-step (canLead=true) unless noted.
//
// Build: g++ -std=c++17 test/test_rule2_integration.cpp
#include <cstdio>
#include <string>
#include <vector>

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) ++passed; else { ++failed; std::printf("  FAIL: %s\n", what); }
}

enum class Mono { NewNote, Legato, Tie, MidNote, Rest };
struct Step { Mono m; float r; bool canLead = true; };  // r = legato draw at THIS voice's cell

// Run a voice through the chain, return its gate log:
//   T = trigger/re-articulate (fresh gate edge), C = connect (slide/extend, no edge),
//   - = hold (MidNote), . = silent.
static std::string run(const std::vector<Step>& seq, float prob) {
    bool participating = false, slurForward = false;
    std::string log;
    auto roll = [&](const Step& s) {
        return s.canLead && (prob >= 0.999f || s.r < prob);
    };
    for (const auto& s : seq) {
        switch (s.m) {
            case Mono::NewNote:                              // onset: play, roll
                participating = true;  slurForward = roll(s); log += 'T'; break;
            case Mono::Rest:                                 // chain ends
                participating = false; log += '.'; break;
            case Mono::MidNote:                              // mid-hold, no edge
                log += '-'; break;
            case Mono::Legato:
            case Mono::Tie: {                                // landing: consume + re-roll
                if (!participating) { log += '.'; break; }
                bool prevSlur = slurForward;
                log += prevSlur ? 'C' : 'T';                 // connect vs re-articulate
                slurForward = roll(s);                       // re-roll for the next landing
                break;
            }
        }
    }
    return log;
}

int main() {
    std::printf("\033[1mRule 2 roll->consume integration\033[0m\n");
    std::printf("%s\n", std::string(46, '=').c_str());

    // A 4-note mono legato run: NewNote then three held landings.
    auto seqWith = [](std::vector<float> r) {
        return std::vector<Step>{
            {Mono::NewNote, r[0]}, {Mono::Legato, r[1]}, {Mono::Legato, r[2]}, {Mono::Legato, r[3]}
        };
    };

    const float prob = 0.5f;

    // Delegated voice reads mono's cells; take draws that all fire (mono slurs the whole run).
    std::string deleg = run(seqWith({0.1f, 0.1f, 0.1f, 0.1f}), prob);
    check(deleg == "TCCC", "delegated voice: slurs the whole run (T C C C)");

    // Local East voice reads its OWN cells — a shifted window yields different draws. With
    // draws straddling the threshold it breaks and re-forms legato independently of mono.
    std::string local = run(seqWith({0.3f, 0.7f, 0.2f, 0.8f}), prob);
    check(local == "TCTC", "Local East voice: re-articulates where its own roll missed");
    check(local != deleg, "=> per-voice LEGATO DIVERGES at intermediate legatoProb");

    // The mechanism is symmetric: another window gives yet another pattern.
    std::string local2 = run(seqWith({0.9f, 0.1f, 0.9f, 0.1f}), prob);
    check(local2 == "TTCT", "a different LEG window gives a different articulation");

    // ── The one case where per-voice legato CANNOT diverge: LegatoMax ──
    // At prob >= 0.999 the roll is forced true for EVERY voice, so all connect regardless of
    // their cell. This is mono behaviour mirrored (LegatoMax forces connection). So a user
    // testing at MAX legato sees no divergence — the sweet spot is intermediate legato.
    {
        std::string dMax = run(seqWith({0.1f, 0.1f, 0.1f, 0.1f}), 1.0f);
        std::string lMax = run(seqWith({0.9f, 0.9f, 0.9f, 0.9f}), 1.0f);  // draws that would MISS at 0.5
        check(dMax == "TCCC" && lMax == "TCCC", "LegatoMax: all voices forced to connect");
        check(dMax == lMax, "=> NO divergence at LegatoMax (>=0.999) — expected, not a bug");
    }

    // ── And at legatoProb 0: nobody slurs (but mono makes no landings either, so N/A live) ──
    {
        std::string z = run(seqWith({0.1f, 0.1f, 0.1f, 0.1f}), 0.0f);
        check(z == "TTTT", "legatoProb 0: never slurs (all re-articulate)");
    }

    // ── A short/fractional note cannot lead: VARIATION can suppress legato via canLead ──
    // If a voice's VAR makes a note fractional, that note forces slurForward=false → the NEXT
    // landing re-articulates regardless of the LEG draw. So VAR (not just LEG) can flatten
    // legato — a thing to rule out when a voice "won't slur".
    {
        std::vector<Step> s = {
            {Mono::NewNote, 0.1f, /*canLead=*/false},  // note can't lead even though r fires
            {Mono::Legato,  0.1f, true}
        };
        check(run(s, prob) == "TT", "fractional (canLead=false) note forces re-articulate next landing");
    }

    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
