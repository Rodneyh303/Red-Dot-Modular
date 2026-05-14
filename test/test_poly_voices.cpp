/**
 * test_poly_voices.cpp  —  Poly voice architecture unit tests
 *
 * Tests MonoDecision propagation, poly voice gate/pitch/tie/legato behaviour,
 * independent rest probability, and numPolyVoices gating.
 * Self-contained — no Rack SDK required.
 *
 * Compile:
 *   g++ -std=c++17 -I. test_poly_voices.cpp -o test_poly && ./test_poly
 */

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>

// ── Test framework ─────────────────────────────────────────────────────────────
static int s_pass = 0, s_fail = 0;
#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc,...) do { \
    bool _ok = true; std::string _msg; \
    try { __VA_ARGS__; } catch (const std::exception& _e) { _ok=false; _msg=_e.what(); } \
    if (_ok) { ++s_pass; std::cout << "  \033[32m✓\033[0m " << (desc) << "\n"; } \
    else     { ++s_fail; std::cout << "  \033[31m✗\033[0m " << (desc); \
               if (!_msg.empty()) std::cout << " — " << _msg; std::cout << "\n"; } \
} while(0)
#define EXPECT(e)      do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)
#define EXPECT_EQ(a,b) do { if((a)!=(b)) { std::ostringstream _s; \
    _s << #a << "=" << (a) << " != " << #b << "=" << (b); \
    throw std::runtime_error(_s.str()); } } while(0)
#define EXPECT_NEAR(a,b,e) do { if(std::fabs((a)-(b))>(e)) { std::ostringstream _s; \
    _s << #a << "=" << (a) << " not near " << #b << "=" << (b); \
    throw std::runtime_error(_s.str()); } } while(0)

// ── Minimal GateState mirror ───────────────────────────────────────────────────
// Mirrors GateState.cpp exactly for the methods used by executePolyVoice.

static float gs_noteSteps(int nvIdx) {
    static const float fracs[9] = {
        0.5f, 0.25f, 1.f/6.f, 0.125f, 1.f/12.f, 0.0625f, 1.f/24.f, 0.03125f, 1.f/48.f
    };
    if (nvIdx < 0 || nvIdx > 8) return 1.f;
    return std::max(0.5f, fracs[nvIdx] * 16.f);
}

struct VoiceState {
    float currentPitchV = 0.f;
    int   lastSemitone  = -1;
    float holdRemain    = 0.f;
    bool  gateHeld      = false;
    bool  gateTriggered = false;  // true when gatePulse.trigger() would be called

    void reset() {
        currentPitchV = 0.f; lastSemitone = -1;
        holdRemain = 0.f; gateHeld = false; gateTriggered = false;
    }

    // Call once per step edge before executePolyVoice.
    void tick() {
        gateTriggered = false;
        if (gateHeld) {
            holdRemain -= 1.f;
            if (holdRemain <= 0.f) { holdRemain = 0.f; gateHeld = false; }
        } else {
            holdRemain = std::max(0.f, holdRemain - 1.f);
        }
    }

    // triggerNote: retrigger, new pitch, new hold.
    void triggerNote(float pitchV, int sem, int nvIdx) {
        currentPitchV = pitchV;
        lastSemitone  = sem;
        gateTriggered = true;
        gateHeld      = true;
        holdRemain    = gs_noteSteps(nvIdx);
    }

    // slideNote: legato — pitch changes, no retrigger (unless cold start).
    void slideNote(float pitchV, int sem, int nvIdx, bool wasHeld) {
        float dur = gs_noteSteps(nvIdx);
        currentPitchV = pitchV;
        lastSemitone  = sem;
        if (wasHeld) {
            holdRemain += dur;
            gateHeld = true;
        } else {
            gateTriggered = true;   // first note of legato run opens gate
            holdRemain    = dur;
            gateHeld      = true;
        }
    }

    // extendHold: tie — same pitch, just extend.
    void extendHold(int sem, int nvIdx) {
        holdRemain += gs_noteSteps(nvIdx);
        gateHeld = true;
        lastSemitone = sem;
    }
};

// ── MonoDecision / StepResult mirrors ─────────────────────────────────────────
enum class MonoDecision {
    MidNote, Rest, Tie, Legato, LegatoMax, NewNote
};

struct StepResult {
    MonoDecision decision = MonoDecision::MidNote;
    int  nvIdx   = 2;   // default: quarter note index
    bool stepped = false;
    bool wrapped = false;
};

// ── PolyVoice mirror ───────────────────────────────────────────────────────────
struct PolyVoice {
    VoiceState gs;
    float restProb = 0.1f;
};

// ── PolyEngine ─────────────────────────────────────────────────────────────────
// Mirrors SequencerEngine's poly execution exactly, with injected RNG and pitch.
struct PolyEngine {
    PolyVoice voices[7];
    int       numPolyVoices = 0;
    StepResult lastStepResult;

    // Injected values consumed by executePolyVoice in order:
    // Each voice draw consumes three values: r_rest, r_semi, r_oct
    float rngSeq[128] = {};
    int   rngIdx      = 0;
    float nextRng()   { float v = rngSeq[rngIdx % 128]; rngIdx++; return v; }

    // Injected pitch/semitone output from genPitchLive mock
    float nextPitchV  = 1.0f;
    int   nextSem     = 5;

    void reset() {
        for (int i = 0; i < 7; ++i) voices[i].gs.reset();
        lastStepResult = StepResult{};
        rngIdx = 0;
        numPolyVoices = 0;
    }

    // Set all RNG values to a constant.
    void setRng(float v) { std::fill(rngSeq, rngSeq + 128, v); rngIdx = 0; }

    // Mirror of executePolyVoice — matches SequencerEngine.cpp exactly.
    void executePolyVoice(int voiceIdx) {
        PolyVoice& v = voices[voiceIdx];
        bool wasHeld = v.gs.gateHeld;
        v.gs.tick();

        switch (lastStepResult.decision) {
            case MonoDecision::MidNote:
                return;
            case MonoDecision::Rest:
                return;
            case MonoDecision::Tie:
                if (wasHeld)
                    v.gs.extendHold(v.gs.lastSemitone, lastStepResult.nvIdx);
                return;
            case MonoDecision::Legato:
            case MonoDecision::LegatoMax:
            case MonoDecision::NewNote:
                break;
        }

        float r_rest = nextRng();
        if (r_rest < v.restProb) return;

        // r_semi and r_oct consumed but result comes from injected nextPitchV/nextSem.
        nextRng(); nextRng();
        int   sem    = nextSem;
        float pitchV = nextPitchV;

        if (lastStepResult.decision == MonoDecision::NewNote) {
            v.gs.triggerNote(pitchV, sem, lastStepResult.nvIdx);
        } else {
            if (sem == v.gs.lastSemitone && wasHeld)
                v.gs.extendHold(sem, lastStepResult.nvIdx);
            else
                v.gs.slideNote(pitchV, sem, lastStepResult.nvIdx, wasHeld);
        }
    }

    void executePolyVoices() {
        for (int i = 0; i < numPolyVoices; ++i)
            executePolyVoice(i);
    }
};

// ═════════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\033[1mPoly Voice Architecture Tests\033[0m\n";
    std::cout << std::string(54, '=') << "\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("MonoDecision::MidNote — poly just ticks, no new note");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("MidNote: voice that was silent stays silent", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.lastStepResult.decision = MonoDecision::MidNote;
        e.executePolyVoice(0);
        EXPECT(!e.voices[0].gs.gateHeld);
        EXPECT(!e.voices[0].gs.gateTriggered);
    });

    TEST("MidNote: voice that was held has hold decremented by tick", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld   = true;
        e.voices[0].gs.holdRemain = 3.f;
        e.lastStepResult.decision = MonoDecision::MidNote;
        e.executePolyVoice(0);
        EXPECT(e.voices[0].gs.gateHeld);
        EXPECT_NEAR(e.voices[0].gs.holdRemain, 2.f, 1e-5f);
        EXPECT(!e.voices[0].gs.gateTriggered);
    });

    TEST("MidNote: hold expires after enough ticks (gate closes)", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld   = true;
        e.voices[0].gs.holdRemain = 2.f;
        e.lastStepResult.decision = MonoDecision::MidNote;
        e.executePolyVoice(0);  // hold = 1
        e.executePolyVoice(0);  // hold = 0 → gate closes
        EXPECT(!e.voices[0].gs.gateHeld);
    });

    TEST("MidNote: pitch is not changed", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.currentPitchV = 2.5f;
        e.voices[0].gs.gateHeld = true; e.voices[0].gs.holdRemain = 2.f;
        e.lastStepResult.decision = MonoDecision::MidNote;
        e.nextPitchV = 4.0f;
        e.executePolyVoice(0);
        EXPECT_NEAR(e.voices[0].gs.currentPitchV, 2.5f, 1e-5f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("MonoDecision::Rest — poly cannot initiate");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Rest: silent voice stays silent", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.lastStepResult.decision = MonoDecision::Rest;
        e.setRng(0.0f);  // would play if it got past the Rest guard
        e.executePolyVoice(0);
        EXPECT(!e.voices[0].gs.gateHeld);
        EXPECT(!e.voices[0].gs.gateTriggered);
    });

    TEST("Rest: held voice has its hold decremented (tick still runs)", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld   = true;
        e.voices[0].gs.holdRemain = 3.f;
        e.lastStepResult.decision = MonoDecision::Rest;
        e.executePolyVoice(0);
        EXPECT_NEAR(e.voices[0].gs.holdRemain, 2.f, 1e-5f);
    });

    TEST("Rest: held voice's note eventually expires through mono rests", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld   = true;
        e.voices[0].gs.holdRemain = 2.f;
        e.lastStepResult.decision = MonoDecision::Rest;
        e.executePolyVoice(0);  // hold = 1
        e.executePolyVoice(0);  // hold = 0 → gate closes
        EXPECT(!e.voices[0].gs.gateHeld);
    });

    TEST("Rest: no new note even when restProb=0.0 (gate guard fires first)", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].restProb = 0.0f;
        e.lastStepResult.decision = MonoDecision::Rest;
        e.setRng(0.0f);  // would draw a note if guard didn't fire
        e.executePolyVoice(0);
        EXPECT(!e.voices[0].gs.gateHeld);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("MonoDecision::Tie — held voices extend, silent voices stay silent");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Tie: held voice extends holdRemain by nvIdx duration", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld     = true;
        e.voices[0].gs.holdRemain   = 2.f;   // will tick to 1 before extend
        e.voices[0].gs.lastSemitone = 5;
        e.lastStepResult.decision   = MonoDecision::Tie;
        e.lastStepResult.nvIdx      = 4;     // sixteenth = 1.0 step
        e.executePolyVoice(0);
        // After tick: holdRemain=1.  extendHold adds gs_noteSteps(4)=1.0  → 2.0
        EXPECT_NEAR(e.voices[0].gs.holdRemain, 2.0f, 1e-4f);
    });

    TEST("Tie: held voice gate stays open after extend", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld   = true;
        e.voices[0].gs.holdRemain = 1.f;
        e.voices[0].gs.lastSemitone = 5;
        e.lastStepResult.decision   = MonoDecision::Tie;
        e.lastStepResult.nvIdx      = 4;
        e.executePolyVoice(0);
        EXPECT(e.voices[0].gs.gateHeld);
    });

    TEST("Tie: no gate retrigger on held voice", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld   = true;
        e.voices[0].gs.holdRemain = 2.f;
        e.voices[0].gs.lastSemitone = 5;
        e.lastStepResult.decision   = MonoDecision::Tie;
        e.lastStepResult.nvIdx      = 4;
        e.executePolyVoice(0);
        EXPECT(!e.voices[0].gs.gateTriggered);
    });

    TEST("Tie: silent voice stays silent (not resurrected)", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        // voice is silent: gateHeld=false
        e.lastStepResult.decision = MonoDecision::Tie;
        e.lastStepResult.nvIdx    = 4;
        e.executePolyVoice(0);
        EXPECT(!e.voices[0].gs.gateHeld);
        EXPECT(!e.voices[0].gs.gateTriggered);
    });

    TEST("Tie: pitch unchanged on held voice", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld     = true;
        e.voices[0].gs.holdRemain   = 2.f;
        e.voices[0].gs.currentPitchV = 2.5f;
        e.voices[0].gs.lastSemitone = 5;
        e.nextPitchV = 9.9f;  // would change pitch if used
        e.lastStepResult.decision   = MonoDecision::Tie;
        e.lastStepResult.nvIdx      = 4;
        e.executePolyVoice(0);
        EXPECT_NEAR(e.voices[0].gs.currentPitchV, 2.5f, 1e-5f);
    });

    TEST("Tie: three consecutive ties keep voice extended", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld   = true;
        e.voices[0].gs.holdRemain = 2.f;
        e.voices[0].gs.lastSemitone = 5;
        e.lastStepResult.decision = MonoDecision::Tie;
        e.lastStepResult.nvIdx    = 4;  // 1.0 step each
        e.executePolyVoice(0);  // tick→1, extend→2
        e.executePolyVoice(0);  // tick→1, extend→2
        e.executePolyVoice(0);  // tick→1, extend→2
        EXPECT(e.voices[0].gs.gateHeld);
        EXPECT_NEAR(e.voices[0].gs.holdRemain, 2.0f, 1e-4f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("MonoDecision::NewNote — poly independently retriggeres");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("NewNote: voice with restProb=0 always triggers", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].restProb = 0.0f;
        e.setRng(0.0f);  // r_rest=0.0 < restProb=0.0 is FALSE → plays
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 4;
        e.nextPitchV = 2.0f; e.nextSem = 7;
        e.executePolyVoice(0);
        EXPECT(e.voices[0].gs.gateHeld);
        EXPECT(e.voices[0].gs.gateTriggered);
        EXPECT_NEAR(e.voices[0].gs.currentPitchV, 2.0f, 1e-5f);
        EXPECT_EQ(e.voices[0].gs.lastSemitone, 7);
    });

    TEST("NewNote: voice with restProb=1 never triggers", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].restProb = 1.0f;
        e.setRng(0.0f);  // r_rest=0.0 < 1.0 → rest
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 4;
        e.executePolyVoice(0);
        EXPECT(!e.voices[0].gs.gateHeld);
        EXPECT(!e.voices[0].gs.gateTriggered);
    });

    TEST("NewNote: uses retrigger (gateTriggered=true) not slide", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].restProb = 0.0f;
        e.voices[0].gs.gateHeld = true;  // even if held, NewNote retriggers
        e.voices[0].gs.holdRemain = 2.f;
        e.setRng(0.0f);
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 4;
        e.nextPitchV = 1.5f; e.nextSem = 3;
        e.executePolyVoice(0);
        EXPECT(e.voices[0].gs.gateTriggered);
    });

    TEST("NewNote: holdRemain set to nvIdx duration", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].restProb = 0.0f;
        e.setRng(0.0f);
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 3;  // eighth = 2.0 steps
        e.nextPitchV = 1.5f; e.nextSem = 5;
        e.executePolyVoice(0);
        EXPECT_NEAR(e.voices[0].gs.holdRemain, 2.0f, 1e-4f);
    });

    TEST("NewNote: independent pitch per voice (each voice has its own nextSem)", {
        // Two voices, same engine state. Since we inject nextPitchV/nextSem
        // they both get the same value here, but the key is they each draw
        // independently (separate rng advancement).
        PolyEngine e; e.reset(); e.numPolyVoices = 2;
        e.voices[0].restProb = 0.0f;
        e.voices[1].restProb = 0.0f;
        e.setRng(0.0f);
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 4;
        e.nextPitchV = 2.0f; e.nextSem = 9;
        e.executePolyVoice(0);
        e.executePolyVoice(1);
        EXPECT(e.voices[0].gs.gateTriggered);
        EXPECT(e.voices[1].gs.gateTriggered);
        EXPECT_EQ(e.rngIdx, 6);  // 3 draws per voice × 2 voices
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("MonoDecision::Legato — no retrigger, poly voice may slide");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Legato: held voice slides to new pitch, no retrigger", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld   = true;
        e.voices[0].gs.holdRemain = 2.f;
        e.voices[0].gs.lastSemitone = 5;
        e.voices[0].restProb = 0.0f;
        e.setRng(0.0f);
        e.lastStepResult.decision = MonoDecision::Legato;
        e.lastStepResult.nvIdx    = 4;
        e.nextPitchV = 3.0f; e.nextSem = 9;  // different semitone → slide
        e.executePolyVoice(0);
        EXPECT(e.voices[0].gs.gateHeld);
        EXPECT(!e.voices[0].gs.gateTriggered);
        EXPECT_NEAR(e.voices[0].gs.currentPitchV, 3.0f, 1e-5f);
    });

    TEST("Legato: held voice extends holdRemain (slide while held)", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld   = true;
        e.voices[0].gs.holdRemain = 2.f;    // after tick: 1.0
        e.voices[0].gs.lastSemitone = 5;
        e.voices[0].restProb = 0.0f;
        e.setRng(0.0f);
        e.lastStepResult.decision = MonoDecision::Legato;
        e.lastStepResult.nvIdx    = 4;      // 1.0 step
        e.nextPitchV = 3.0f; e.nextSem = 9;
        e.executePolyVoice(0);
        // tick: holdRemain 2→1, then slideNote(wasHeld=true) extends: 1+1=2
        EXPECT_NEAR(e.voices[0].gs.holdRemain, 2.0f, 1e-4f);
    });

    TEST("Legato: cold voice (not held) opens gate with retrigger", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        // voice is cold — gateHeld = false
        e.voices[0].restProb = 0.0f;
        e.setRng(0.0f);
        e.lastStepResult.decision = MonoDecision::Legato;
        e.lastStepResult.nvIdx    = 4;
        e.nextPitchV = 2.5f; e.nextSem = 7;
        e.executePolyVoice(0);
        // slideNote(wasHeld=false) opens gate with pulse
        EXPECT(e.voices[0].gs.gateHeld);
        EXPECT(e.voices[0].gs.gateTriggered);
    });

    TEST("Legato: voice resting by restProb stays silent", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].restProb = 1.0f;
        e.setRng(0.0f);  // r_rest=0.0 < 1.0 → rest
        e.lastStepResult.decision = MonoDecision::Legato;
        e.lastStepResult.nvIdx    = 4;
        e.executePolyVoice(0);
        EXPECT(!e.voices[0].gs.gateHeld);
    });

    TEST("LegatoMax: behaves identically to Legato for poly voice", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld   = true;
        e.voices[0].gs.holdRemain = 2.f;
        e.voices[0].gs.lastSemitone = 5;
        e.voices[0].restProb = 0.0f;
        e.setRng(0.0f);
        e.lastStepResult.decision = MonoDecision::LegatoMax;
        e.lastStepResult.nvIdx    = 4;
        e.nextPitchV = 3.5f; e.nextSem = 11;
        e.executePolyVoice(0);
        EXPECT(!e.voices[0].gs.gateTriggered);
        EXPECT_NEAR(e.voices[0].gs.currentPitchV, 3.5f, 1e-5f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Poly-internal tie — legato zone, same semitone as previous");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Legato + same semitone + held: poly-internal tie (extendHold)", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld     = true;
        e.voices[0].gs.holdRemain   = 2.f;
        e.voices[0].gs.lastSemitone = 7;
        e.voices[0].gs.currentPitchV = 2.0f;
        e.voices[0].restProb = 0.0f;
        e.setRng(0.0f);
        e.lastStepResult.decision = MonoDecision::Legato;
        e.lastStepResult.nvIdx    = 4;   // 1.0 step
        e.nextSem = 7;   // SAME as lastSemitone → poly-internal tie
        e.nextPitchV = 2.0f;
        e.executePolyVoice(0);
        // tick: holdRemain 2→1; extendHold adds 1 → holdRemain=2
        EXPECT(!e.voices[0].gs.gateTriggered);
        EXPECT(e.voices[0].gs.gateHeld);
        EXPECT_NEAR(e.voices[0].gs.holdRemain, 2.0f, 1e-4f);
    });

    TEST("Legato + same semitone + NOT held: opens gate (slide, not tie)", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld     = false;  // cold voice
        e.voices[0].gs.lastSemitone = 7;
        e.voices[0].restProb = 0.0f;
        e.setRng(0.0f);
        e.lastStepResult.decision = MonoDecision::Legato;
        e.lastStepResult.nvIdx    = 4;
        e.nextSem    = 7;  // same semitone, but wasHeld=false → slide, not tie
        e.nextPitchV = 2.0f;
        e.executePolyVoice(0);
        // wasHeld=false → slideNote opens gate with pulse
        EXPECT(e.voices[0].gs.gateTriggered);
    });

    TEST("Legato + different semitone: slide (not tie) even if held", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld     = true;
        e.voices[0].gs.holdRemain   = 2.f;
        e.voices[0].gs.lastSemitone = 5;
        e.voices[0].restProb = 0.0f;
        e.setRng(0.0f);
        e.lastStepResult.decision = MonoDecision::Legato;
        e.lastStepResult.nvIdx    = 4;
        e.nextSem    = 9;   // DIFFERENT → slide
        e.nextPitchV = 3.0f;
        e.executePolyVoice(0);
        EXPECT(!e.voices[0].gs.gateTriggered);
        EXPECT_EQ(e.voices[0].gs.lastSemitone, 9);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("numPolyVoices — only active voices are executed");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("numPolyVoices=0: executePolyVoices does nothing", {
        PolyEngine e; e.reset();
        e.numPolyVoices = 0;
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 4;
        e.setRng(0.0f);
        e.executePolyVoices();
        // No voice should have changed
        for (int i = 0; i < 7; ++i) {
            EXPECT(!e.voices[i].gs.gateHeld);
            EXPECT(!e.voices[i].gs.gateTriggered);
        }
        EXPECT_EQ(e.rngIdx, 0);  // no RNG draws made
    });

    TEST("numPolyVoices=3: exactly voices 0,1,2 are executed", {
        PolyEngine e; e.reset();
        e.numPolyVoices = 3;
        for (int i = 0; i < 7; ++i) e.voices[i].restProb = 0.0f;
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 4;
        e.setRng(0.0f);
        e.nextPitchV = 1.0f; e.nextSem = 5;
        e.executePolyVoices();
        EXPECT(e.voices[0].gs.gateTriggered);
        EXPECT(e.voices[1].gs.gateTriggered);
        EXPECT(e.voices[2].gs.gateTriggered);
        EXPECT(!e.voices[3].gs.gateTriggered);
        EXPECT(!e.voices[4].gs.gateTriggered);
    });

    TEST("numPolyVoices=7: all voices executed", {
        PolyEngine e; e.reset();
        e.numPolyVoices = 7;
        for (int i = 0; i < 7; ++i) e.voices[i].restProb = 0.0f;
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 4;
        e.setRng(0.0f);
        e.nextPitchV = 1.0f; e.nextSem = 5;
        e.executePolyVoices();
        for (int i = 0; i < 7; ++i)
            EXPECT(e.voices[i].gs.gateTriggered);
        EXPECT_EQ(e.rngIdx, 21);  // 3 draws × 7 voices
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Independent rest probability per voice");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Voice 0 plays, voice 1 rests (different restProb)", {
        PolyEngine e; e.reset(); e.numPolyVoices = 2;
        e.voices[0].restProb = 0.0f;  // always plays
        e.voices[1].restProb = 1.0f;  // always rests
        e.setRng(0.0f);  // r_rest=0 → 0<0 is false (v0 plays), 0<1 is true (v1 rests)
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 4;
        e.nextPitchV = 1.0f; e.nextSem = 5;
        e.executePolyVoices();
        EXPECT(e.voices[0].gs.gateTriggered);
        EXPECT(!e.voices[1].gs.gateTriggered);
    });

    TEST("All voices at restProb=0.5: voice plays iff r_rest >= 0.5", {
        PolyEngine e; e.reset(); e.numPolyVoices = 3;
        for (int i = 0; i < 3; ++i) e.voices[i].restProb = 0.5f;
        // Inject: voice0 r_rest=0.4 (plays), voice1 r_rest=0.6 (rests), voice2 r_rest=0.49 (plays)
        e.rngSeq[0] = 0.4f;  e.rngSeq[1] = 0.0f; e.rngSeq[2] = 0.0f;  // v0: plays
        e.rngSeq[3] = 0.6f;  e.rngSeq[4] = 0.0f; e.rngSeq[5] = 0.0f;  // v1: rests
        e.rngSeq[6] = 0.49f; e.rngSeq[7] = 0.0f; e.rngSeq[8] = 0.0f;  // v2: plays
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 4;
        e.nextPitchV = 1.0f; e.nextSem = 5;
        e.executePolyVoices();
        EXPECT( e.voices[0].gs.gateTriggered);
        EXPECT(!e.voices[1].gs.gateTriggered);
        EXPECT( e.voices[2].gs.gateTriggered);
    });

    TEST("restProb boundary: r_rest exactly equals restProb → plays (strict less-than)", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].restProb = 0.5f;
        e.rngSeq[0] = 0.5f;  // r_rest = 0.5, restProb = 0.5 → NOT less-than → plays
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 4;
        e.nextPitchV = 1.0f; e.nextSem = 5;
        e.executePolyVoice(0);
        EXPECT(e.voices[0].gs.gateTriggered);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("RNG consumption — correct draws per decision");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("MidNote: zero RNG draws", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.lastStepResult.decision = MonoDecision::MidNote;
        e.executePolyVoice(0);
        EXPECT_EQ(e.rngIdx, 0);
    });

    TEST("Rest: zero RNG draws", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.lastStepResult.decision = MonoDecision::Rest;
        e.executePolyVoice(0);
        EXPECT_EQ(e.rngIdx, 0);
    });

    TEST("Tie: zero RNG draws (no pitch draw needed)", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].gs.gateHeld = true; e.voices[0].gs.holdRemain = 2.f;
        e.voices[0].gs.lastSemitone = 5;
        e.lastStepResult.decision = MonoDecision::Tie;
        e.lastStepResult.nvIdx    = 4;
        e.executePolyVoice(0);
        EXPECT_EQ(e.rngIdx, 0);
    });

    TEST("NewNote that plays: exactly 3 RNG draws (r_rest + r_semi + r_oct)", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].restProb = 0.0f;
        e.setRng(0.0f);
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 4;
        e.nextPitchV = 1.0f; e.nextSem = 5;
        e.executePolyVoice(0);
        EXPECT_EQ(e.rngIdx, 3);
    });

    TEST("NewNote that rests: exactly 1 RNG draw (r_rest only)", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].restProb = 1.0f;
        e.setRng(0.0f);
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.lastStepResult.nvIdx    = 4;
        e.executePolyVoice(0);
        EXPECT_EQ(e.rngIdx, 1);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Full sequence — interleaved mono decisions across 16 steps");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Poly voice matches pattern: plays on NewNote, holds on Tie, silent on Rest", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].restProb = 0.0f;  // always plays when allowed
        e.setRng(0.0f);
        e.nextPitchV = 1.5f; e.nextSem = 7;
        e.lastStepResult.nvIdx = 4;  // 1.0 step

        // Step 1: NewNote → triggers
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.executePolyVoice(0);
        EXPECT(e.voices[0].gs.gateTriggered);
        EXPECT(e.voices[0].gs.gateHeld);

        // Step 2: Tie → extends
        e.lastStepResult.decision = MonoDecision::Tie;
        e.executePolyVoice(0);
        EXPECT(!e.voices[0].gs.gateTriggered);
        EXPECT(e.voices[0].gs.gateHeld);

        // Step 3: Rest → hold decays (tick), no new note
        e.lastStepResult.decision = MonoDecision::Rest;
        float holdBefore = e.voices[0].gs.holdRemain;
        e.executePolyVoice(0);
        EXPECT(!e.voices[0].gs.gateTriggered);
        EXPECT(e.voices[0].gs.holdRemain < holdBefore);

        // Step 4: NewNote → retriggers
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.executePolyVoice(0);
        EXPECT(e.voices[0].gs.gateTriggered);
    });

    TEST("Legato run: pitch evolves across steps, gate never retriggered after first", {
        PolyEngine e; e.reset(); e.numPolyVoices = 1;
        e.voices[0].restProb = 0.0f;
        e.setRng(0.0f);
        e.lastStepResult.nvIdx = 4;

        // Step 1: NewNote opens gate
        e.lastStepResult.decision = MonoDecision::NewNote;
        e.nextPitchV = 1.0f; e.nextSem = 3;
        e.executePolyVoice(0);
        EXPECT(e.voices[0].gs.gateTriggered);

        // Steps 2-4: Legato with different semitones — slide, no retrigger
        for (int step = 2; step <= 4; ++step) {
            e.lastStepResult.decision = MonoDecision::Legato;
            e.nextPitchV = float(step) * 0.5f;
            e.nextSem = step + 2;  // always different from previous
            e.executePolyVoice(0);
            EXPECT(!e.voices[0].gs.gateTriggered);
            EXPECT(e.voices[0].gs.gateHeld);
        }
    });

    // ─────────────────────────────────────────────────────────────────────────
    std::cout << "\n" << std::string(54, '=') << "\n";
    std::cout << "\033[32m" << s_pass << " passed\033[0m  ";
    if (s_fail) std::cout << "\033[31m" << s_fail << " failed\033[0m";
    else        std::cout << "\033[32m0 failed\033[0m";
    std::cout << "  (" << (s_pass + s_fail) << " total)\n\n";
    return s_fail ? 1 : 0;
}
