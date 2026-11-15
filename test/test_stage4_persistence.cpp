// test_stage4_persistence.cpp — STEPS_32 Stage 4 — the control + persistence gate.
//
// The plan (docs/design/STEPS_32_PLAN.md, "Stage 4 — control + persistence"):
//   "Pattern-length parameter, persisted; absent key → 16. Gate: old patches load
//    at 16 unchanged; round-trip 32."
// and the revised restatement: "Patch compatibility is not a concern for this
// unreleased code (decided); the gate is behaviour at ≤16, not file compatibility."
//
// WHERE THE LAW LIVES TODAY. The length control is and stays PATTERN_LENGTH_PARAM
// (the MODE LAW: no 16/32 switch, no second copy of the information). Rack itself
// persists module params: Module::toJson serializes every paramQuantity into the
// patch's "params" array and Module::fromJson restores them BEFORE the plugin's
// dataFromJson("data") hook runs (the persistence manager's own comments document
// the order — the legacy note-value migration relies on it). An absent param entry
// therefore restores the configParam default: 16 (D2: "Defaults stay 16"). The
// stages already in place carry the rest of the chain: the knob's range derives
// from kPatternLengthCap (the Stage-1 close-out), the cap is 32 (the 2b flip), the
// window math is cap-derived (updateWindow: knob clamp 1..cap, the D1 *31 CV map,
// the ring-derived offset domain), and the lane LOR pushes clamp cap-derived.
//
// WHAT THIS SUITE OWNS: NOTHING yet pins any of that. A future change could
// re-derive the range from the ceiling, add a writer that snaps the knob on load,
// leave a residual 16-clamp in the restore path, or add a SECOND persisted copy of
// the length (a json key that can disagree with the knob — the exact failure the
// MODE LAW's save/restore argument rules out) — and no test would trip. This gate
// pins the persistence contract at both of its real layers:
//
//   S0  THE VALUE LAYER — Rack stores params as json reals (doubles). Pin that the
//       knob's whole domain (1..32 integer values, plus mid-drag fractional samples)
//       round-trips the double encoding EXACTLY and lands on the same window
//       request — a saved 32 restores as 32, a saved 16 restores as 16, a mid-drag
//       17.4 does not shift its round() landing.
//
//   S1  THE SOURCE FACTS (the structural grep-gate, the test_module_lookup L5
//       precedent) — MonsoonConfigurator holds EXACTLY ONE configParam site per
//       param and its spelling is the cap-derived, default-16/0 one; NO source line
//       anywhere writes PATTERN_LENGTH_PARAM or PATTERN_OFFSET_PARAM (the knob's
//       only writers are the user's hand and Rack's patch restore — a mode
//       controller or a load hook that snaps the param is the failure class); and
//       NO second persisted home for the length exists (no "patternLength" json
//       key anywhere in src/ — one value, one home, the knob).
//
//   S2  ABSENT KEY → 16 (the default world) — the default push (updateWindow with
//       the knob's default 16, unpatched — Monsoon's per-block call) lands the
//       (16,0) window, the live walk stays inside [0,16) (GATE 1's equivalence
//       domain), and a full varied march under the default push is BYTE-IDENTICAL
//       to the same march with the window authored directly — the restore path
//       cannot drift the default world.
//
//   S3  ROUND-TRIP 32 — the restored float drives the engine identically to the
//       authored window, over the WHOLE restored domain: lengths 1..32 x offsets
//       {0,7,31} all land exactly (L, off) with windowMask == windowMaskBits(L,off)
//       (no residual clamp anywhere in 1..32 — the newly deliverable 17..32
//       included); at 32 the walk wraps at 32 and both bars are reachable; and the
//       full executor marches (mode A + all 15 poly voices, varied rows) at the
//       restored (32,0) and (32,7) pushes are BIT-IDENTICAL to their authored
//       twins — the Philox draw stream cannot shift on a patch reload.
//
//   S4  OLD PATCHES AT ≤16 UNCHANGED — the same restore-transparency over the old
//       patch domain: lengths 1..16 x offsets {0,5,15} land exactly; and the
//       old-patch wrapping configs — (12,10) (crosses the bar line, the D3
//       extension) and (16,5) (the full old ring shifted) — march BIT-IDENTICALLY
//       restored vs authored. GATE 1 already owns the cross-flip equivalence
//       (non-wrapping ≤16 configs equal the 2a-tip oracle); THIS section owns the
//       restore path itself: a loaded old patch behaves EXACTLY like the window it
//       names, at this build.
//
// All run-time comparisons are engine-vs-engine (restored push vs authored
// setWindow) — no recorded oracle, no cross-cap domain bookkeeping. The capture is
// the differential-harness discipline: FNV-1a over raw float BITS (a last-bit drift
// trips), per-step tee digests + the whole-march digest.
//
// Build (test/run_all.sh entry "test_stage4_persistence|$SE $GS $PE $CE").

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <set>
#include <functional>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "PhiloxRng.hpp"

using redDot::PhiloxRng;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

// ── FNV-1a 64 + capture stream (the differential oracle's discipline) ──────────
static uint64_t fnv1a(uint64_t h, uint8_t b) { return (h ^ (uint64_t)b) * 0x100000001b3ULL; }
struct Capture {
    std::vector<uint8_t> bytes;
    void u8(unsigned v)  { bytes.push_back((uint8_t)(v & 0xFFu)); }
    void i16(int v) {
        int16_t s = (int16_t)v;
        bytes.push_back((uint8_t)((s >> 0) & 0xFF));
        bytes.push_back((uint8_t)((s >> 8) & 0xFF));
    }
    void u32(uint32_t v) {
        bytes.push_back((uint8_t)((v >> 0)  & 0xFF));
        bytes.push_back((uint8_t)((v >> 8)  & 0xFF));
        bytes.push_back((uint8_t)((v >> 16) & 0xFF));
        bytes.push_back((uint8_t)((v >> 24) & 0xFF));
    }
    void u64(uint64_t v) { u32((uint32_t)v); u32((uint32_t)(v >> 32)); }
    void f32(float f) { uint32_t b; std::memcpy(&b, &f, 4); u32(b); }   // raw bits
    uint64_t digest() const {
        uint64_t h = 0xcbf29ce484222325ULL;
        for (size_t i = 0; i < bytes.size(); ++i) h = fnv1a(h, bytes[i]);
        return h;
    }
    void clear() { bytes.clear(); }
};

struct TeeCapture {
    Capture step, full;
    std::vector<uint64_t>* steps = nullptr;
    void commit() {
        if (steps) steps->push_back(step.digest());
        full.bytes.insert(full.bytes.end(), step.bytes.begin(), step.bytes.end());
    }
};

static int q12(float pitchV) { return (int)std::lrintf((double)pitchV * 12.0); }

// ── Per-step capture: decisions, gates, CVs, walks, the window state itself ────
static void captureStep(const SequencerEngine& eng, const StepResult& r, Capture& cap) {
    cap.u8((unsigned)r.decision);
    cap.u8((unsigned)r.nvIdx);
    cap.u8(r.accented ? 1u : 0u);
    cap.u8(r.stepped ? 1u : 0u);
    cap.u8(eng.gs.gateHeld ? 1u : 0u);
    cap.u8(eng.gsStep.gateHeld ? 1u : 0u);
    cap.i16(q12(eng.gs.currentPitchV));
    for (int v = 2; v <= 16; ++v) {
        const auto& pv = eng.voices[dotModular::VoiceResolver::slot(v)];
        cap.u8(pv.gs.gateHeld ? 1u : 0u);
        cap.u8(pv.gsStep.gateHeld ? 1u : 0u);
        cap.u8(pv.accented ? 1u : 0u);
        cap.u8(pv.participating ? 1u : 0u);
        cap.i16(q12(pv.gs.currentPitchV));
    }
    // The window state IS the persistence subject: the restored push must land it
    // identically to the authored call, and everything that reads it must follow.
    for (int s = 0; s < 7; ++s) cap.i16(eng.laneTick_[s]);
    cap.i16(eng.globalWalk_);
    cap.i16(eng.cachedLength);
    cap.i16(eng.cachedOffset);
    cap.i16(eng.startStep);
    cap.i16(eng.endStep);
    cap.u64(eng.windowMask);
    cap.u64(eng.effectiveOutputMaskMacro());
    for (int v = 1; v <= 16; ++v)
        cap.u64(eng.effectiveOutputMask(dotModular::VoiceResolver::slot(v)));
}

// ── The marches (the REAL executors; the GATE-1 harness's shape, lean capture) ──
static const int kSteps = 64;

struct GateStream {
    bool high = false;
    void step(PhiloxRng& rng, float pRise, float pFall, bool& rose) {
        rose = false;
        if (high) { if (rng.nextFloat() < pFall) high = false; }
        else      { if (rng.nextFloat() < pRise) { high = true; rose = true; } }
    }
};

struct MarchCtx {
    SequencerEngine eng;
    PatternInput    in;
    float           noteVal = 2.f;
    GateStream      main;
    PhiloxRng       rng;
};

static PatternInput makeInput() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = 0.42f;
    in.octaveLo = 0; in.octaveHi = 1;
    return in;
}

// Varied rows over the FULL cap (all 32 cells varied — a bar-2 mis-landing reads a
// real varied cell, not a zero). Same seed + same call order ⇒ two engines byte-
// identical. The per-voice probabilities ride along (the poly decisions varied).
static void seedVariedRows(SequencerEngine& eng, PhiloxRng& rng) {
    for (int i = 0; i < dotModular::kPatternLengthCap; ++i) {
        eng.pe.melodyRandom[i]    = rng.nextFloat();
        eng.pe.octaveRandom[i]    = rng.nextFloat();
        eng.pe.qmixRandom[i]      = rng.nextFloat();
        eng.pe.rhythmRandom[i]    = rng.nextFloat();
        eng.pe.accentRandom[i]    = rng.nextFloat();
        eng.pe.variationRandom[i] = rng.nextFloat();
        eng.pe.legatoRandom[i]    = rng.nextFloat();
    }
    for (int v = 1; v <= 16; ++v) {
        const auto vs = dotModular::VoiceResolver::slot(v);
        for (int lane = 0; lane < 7; ++lane)
            for (int i = 0; i < dotModular::kPatternLengthCap; ++i)
                eng.pe.polyRandom(vs, dotModular::LaneId{lane})[i] = rng.nextFloat();
        eng.voices[vs].restProb      = 0.05f + 0.60f * rng.nextFloat();
        eng.voices[vs].accentProb    = 0.55f * rng.nextFloat();
        eng.voices[vs].legatoProb    = 0.80f * rng.nextFloat();
        eng.voices[vs].variationProb = 0.20f + 0.55f * rng.nextFloat();
        eng.voices[vs].qmixLevel     = rng.nextFloat();
    }
}

static void marchModeA(MarchCtx& c, TeeCapture& t) {
    redDot::RateManager rm;
    for (int step = 0; step < kSteps; ++step) {
        rm.advance(redDot::Rate::Step, true);
        StepResult r = c.eng.executeModeA(rm, c.noteVal, c.in, +1);
        if (r.stepped && c.eng.numPolyVoices > 0) c.eng.executePolyVoices(c.in);
        captureStep(c.eng, r, t.step); t.commit(); t.step.clear();
    }
}

// A/B harness: two engines from identical seeds; the window arrives on A via the
// RESTORED param push (the values Rack's restore feeds updateWindow), on B via the
// AUTHORED setWindow — then identical marches. Any drift in the restore path
// (landing, mask, walk re-anchor, draw stream) separates the digests.
struct TwinMarch {
    MarchCtx a, b;
    uint64_t digestA = 0, digestB = 0;
    std::vector<uint64_t> stepsA, stepsB;
    TwinMarch(uint64_t seed) {
        a.in = makeInput(); b.in = makeInput();
        a.eng.numPolyVoices = 15; b.eng.numPolyVoices = 15;
        a.eng.reset(); b.eng.reset();
        a.rng.seed64(seed); b.rng.seed64(seed);
        PhiloxRng rowsA(seed + 1), rowsB(seed + 1);
        seedVariedRows(a.eng, rowsA);
        seedVariedRows(b.eng, rowsB);
        stepsA.reserve(kSteps); stepsB.reserve(kSteps);
    }
    void restoredPush(float L, float O) {   // the module's per-block call shape
        a.eng.updateWindow(L, 0.f, false, O, 0.f, false);
    }
    void authoredWindow(int L, int O) { b.eng.setWindow(L, O); }
    void run() {
        TeeCapture ta, tb;
        ta.steps = &stepsA; tb.steps = &stepsB;
        marchModeA(a, ta); marchModeA(b, tb);
        digestA = ta.full.digest(); digestB = tb.full.digest();
    }
    bool identical() const { return digestA == digestB && stepsA == stepsB; }
};

// ── S1: the source-facts walker (the test_module_lookup L5 grep-gate pattern) ───
static std::string slurp(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}
struct SrcLine { std::string file; int line; std::string text; };
static std::vector<SrcLine> walkSrc(const std::function<bool(const std::string&)>& hit) {
    std::vector<SrcLine> out;
    const std::filesystem::path root = std::filesystem::current_path();
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root / "src")) {
        if (!entry.is_regular_file()) continue;
        const std::string ext = entry.path().extension().string();
        if (ext != ".cpp" && ext != ".hpp" && ext != ".ipp") continue;
        const std::string rel = std::filesystem::relative(entry.path(), root).generic_string();
        if (rel.rfind("src/deprecated/", 0) == 0) continue;   // dead code — not in the build
        const std::string src = slurp(entry.path());
        int lineNo = 0;
        size_t pos = 0;
        while (pos <= src.size()) {
            const size_t eol = src.find('\n', pos);
            std::string line = src.substr(pos, (eol == std::string::npos ? src.size() : eol) - pos);
            ++lineNo;
            if (hit(line)) out.push_back({rel, lineNo, line});
            if (eol == std::string::npos) break;
            pos = eol + 1;
        }
    }
    return out;
}
static int countIn(const std::string& hay, const char* needle) {
    int n = 0;
    for (size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + 1)) ++n;
    return n;
}

int main() {
    std::printf("test_stage4_persistence — the length param's persistence law "
                "(plan: 'absent key → 16; old patches load at 16 unchanged; round-trip 32')\n");

    // ── 0. the standing constants (the law's frame) ─────────────────────────────
    check(dotModular::kPatternLengthCap == 32, "kPatternLengthCap == 32 (the 2b flip; the knob's max)");
    check(dotModular::kMaskRing == 32,         "kMaskRing == 32 (the offset/ring domain)");

    // ── S0. the VALUE layer — the json real (double) round-trip ────────────────
    // Rack encodes a param as a json real (double); the restore decodes back to
    // float. Pin: every integer knob value 1..32 round-trips EXACTLY and lands on
    // itself; every mid-drag fractional sample keeps its round() landing (the
    // landing is a pure function of the stored value — a reload cannot shift it).
    {
        bool intRT = true;
        for (int L = 1; L <= dotModular::kPatternLengthCap; ++L) {
            float  f   = (float)L;
            double enc = (double)f;          // json_real
            float  back = (float)enc;        // the restored param
            intRT = intRT && back == f && std::lround(back) == L;
        }
        check(intRT, "every integer length 1..32 round-trips the json-double encoding exactly and lands on itself");
        bool fracRT = true;
        const float fracs[] = { 1.0f, 2.71828f, 8.4999f, 16.5f, 17.4f, 17.5f, 23.97f, 31.7f };
        for (float f : fracs)
            fracRT = fracRT && std::lround((float)(double)f) == std::lround(f);
        check(fracRT, "mid-drag fractional values keep their round() landing through the double round-trip");
        // and the offset domain 0..31 rides the same law
        bool offRT = true;
        for (int O = 0; O < dotModular::kMaskRing; ++O) {
            float f = (float)O;
            offRT = offRT && (float)(double)f == f && std::lround(f) == O;
        }
        check(offRT, "every offset 0..31 round-trips the json-double encoding exactly");
    }

    // ── S1. the SOURCE facts (the structural grep-gate) ─────────────────────────
    {
        // Law A: exactly ONE configParam site per param, the cap-derived spelling,
        // the D2 defaults (length 16, offset 0) — the range never re-derives from
        // the ceiling, the default never drifts from 16.
        const std::filesystem::path root = std::filesystem::current_path();
        const std::string cfg = slurp(root / "src/dsp/managers/MonsoonConfigurator.cpp");
        check(countIn(cfg, "configParam(PATTERN_LENGTH_PARAM") == 1,
              "exactly ONE configParam site for PATTERN_LENGTH_PARAM (MonsoonConfigurator)");
        check(countIn(cfg, "configParam(PATTERN_OFFSET_PARAM") == 1,
              "exactly ONE configParam site for PATTERN_OFFSET_PARAM (MonsoonConfigurator)");
        check(cfg.find("configParam(PATTERN_LENGTH_PARAM, 1.f, (float)dotModular::kPatternLengthCap, 16.f") != std::string::npos,
              "the length param's spelling: range 1..kPatternLengthCap (cap-derived), default 16 (D2)");
        check(cfg.find("configParam(PATTERN_OFFSET_PARAM, 0.f, (float)(dotModular::kPatternLengthCap - 1), 0.f") != std::string::npos,
              "the offset param's spelling: range 0..cap-1 (cap-derived), default 0");
        // and NOWHERE else in src/ is a length/offset param configured
        auto strays = walkSrc([](const std::string& l) {
            return l.find("configParam(PATTERN_LENGTH_PARAM") != std::string::npos
                || l.find("configParam(PATTERN_OFFSET_PARAM")  != std::string::npos;
        });
        int straySites = 0;
        for (const auto& r : strays)
            if (r.file != "src/dsp/managers/MonsoonConfigurator.cpp") ++straySites;
        check(straySites == 0,
              "no configParam site for the length/offset params outside MonsoonConfigurator");

        // Law B: the knob's only writers are the user's hand and Rack's patch
        // restore — NO source line writes the param (a mode controller or a load
        // hook that snaps the knob on load is the failure class this law rules out;
        // Rack's own paramsFromJson/Module::onReset are framework-side, not sites).
        auto writers = walkSrc([](const std::string& l) {
            static const char* const hits[] = {
                "PATTERN_LENGTH_PARAM].setValue",  "PATTERN_LENGTH_PARAM].set(",
                "PATTERN_LENGTH_PARAM].setImmediateValue", "PATTERN_LENGTH_PARAM].reset",
                "PATTERN_OFFSET_PARAM].setValue",  "PATTERN_OFFSET_PARAM].set(",
                "PATTERN_OFFSET_PARAM].setImmediateValue", "PATTERN_OFFSET_PARAM].reset",
            };
            for (const char* h : hits) if (l.find(h) != std::string::npos) return true;
            return false;
        });
        check(writers.empty(), "no source line writes PATTERN_LENGTH_PARAM / PATTERN_OFFSET_PARAM "
                               "(the knob answers only to the user and Rack's patch restore)");
        for (size_t i = 0; i < writers.size() && i < 8; ++i)
            std::printf("        writer: %s:%d\n", writers[i].file.c_str(), writers[i].line);

        // Law C: ONE value, ONE home — no second persisted copy of the length (a
        // json key that could disagree with the knob is the exact failure the MODE
        // LAW's save/restore argument rules out).
        auto secondHome = walkSrc([](const std::string& l) {
            return l.find("\"patternLength\"") != std::string::npos
                || l.find("\"pattern_length\"") != std::string::npos
                || l.find("\"patternOffset\"")  != std::string::npos;
        });
        check(secondHome.empty(), "no second persisted home for the length/offset (the MODE LAW: "
                                  "the param is the only copy of the information)");
        for (size_t i = 0; i < secondHome.size() && i < 8; ++i)
            std::printf("        key: %s:%d\n", secondHome[i].file.c_str(), secondHome[i].line);
    }

    // ── S2. ABSENT KEY → 16 — the default world ─────────────────────────────────
    {
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 15;
        // The absent-entry restore = the module's first per-block push with the
        // knob's default value (16, 0) — the exact call Monsoon.cpp makes.
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);
        check(eng.cachedLength == 16 && eng.cachedOffset == 0,
              "the default push lands (16,0) — the absent-key world is the D2 default");
        check(eng.windowMask == SequencerEngine::windowMaskBits(16, 0),
              "the default window mask is cells 0..15 (the live ≤16 world GATE 1 covers)");
        std::vector<int> visited;
        for (int i = 0; i < 40; ++i) {
            eng.advancePlayhead(+1);
            visited.push_back(eng.stepIndex);
        }
        bool inWindow = true;
        for (int s : visited) inWindow = inWindow && (s >= 0 && s < 16);
        check(inWindow, "the absent-key walk stays inside [0,16) (the old patches' world)");
        // and the default world is drift-free end to end: the restored-default push
        // marches BYTE-IDENTICALLY to the authored (16,0) window.
        TwinMarch tm(0x575A34ULL);
        tm.restoredPush(16.f, 0.f);
        tm.authoredWindow(16, 0);
        tm.run();
        check(tm.identical(), "the default push's full march is BIT-IDENTICAL to the authored (16,0) window");
    }

    // ── S3. ROUND-TRIP 32 — the restored domain is fully deliverable ────────────
    {
        // The whole knob domain through the restore call: every (L, off) lands
        // EXACTLY — no residual 16-clamp anywhere in 1..32 (the Stage-1 fix's live
        // face: the knob never offers, and the restore never clamps, an
        // undeliverable length).
        bool sweepOK = true;
        for (int L = 1; L <= dotModular::kPatternLengthCap; ++L) {
            for (int off : {0, 7, 31}) {
                SequencerEngine eng; eng.reset();
                eng.updateWindow((float)L, 0.f, false, (float)off, 0.f, false);
                const uint64_t want = SequencerEngine::windowMaskBits(L, off);
                const int endWant = (off + L - 1) & (dotModular::kMaskRing - 1);
                sweepOK = sweepOK
                    && eng.cachedLength == L && eng.cachedOffset == off
                    && eng.startStep == off && eng.endStep == endWant
                    && eng.windowMask == want;
                if (!sweepOK) {
                    std::printf("        restored (L=%d, off=%d) -> (%d,%d) mask want=%016llx got=%016llx\n",
                                L, off, eng.cachedLength, eng.cachedOffset,
                                (unsigned long long)want, (unsigned long long)eng.windowMask);
                    break;
                }
            }
            if (!sweepOK) break;
        }
        check(sweepOK, "lengths 1..32 x offsets {0,7,31} restore EXACTLY (landing, start/end, mask == windowMaskBits)");
        // at 32: the restored knob drives the ring — wrap at 32, both bars reachable.
        {
            SequencerEngine eng; eng.reset();
            eng.updateWindow(32.f, 0.f, false, 0.f, 0.f, false);
            std::set<int> seen;
            bool bar1 = false, bar2 = false, inside = true;
            for (int i = 0; i < 32; ++i) {
                eng.advancePlayhead(+1);
                inside = inside && eng.stepIndex >= 0 && eng.stepIndex < 32;
                seen.insert(eng.stepIndex);
                if (eng.stepIndex < 16) bar1 = true; else bar2 = true;
            }
            check(inside, "the restored 32-window walk stays inside the 32-ring");
            check((int)seen.size() == 32, "the restored 32-window visitation is ALL 32 cells (wraps at 32, not 16)");
            check(bar1 && bar2, "both bars reachable under the restored 32-window");
        }
        // The bit-identical twins: restored pushes vs authored windows at the
        // round-trip-32 face. (32,0) is the plain reload; (32,7) also re-anchors
        // the walk mid-ring (startStep 7) — the walk re-bound through the restore
        // must match the authored re-bound exactly.
        {
            TwinMarch t0(0xA5A511ULL);
            t0.restoredPush(32.f, 0.f); t0.authoredWindow(32, 0); t0.run();
            check(t0.identical(), "the restored (32,0) push marches BIT-IDENTICALLY to the authored (32,0) window");
            TwinMarch t7(0xC3C322ULL);
            t7.restoredPush(32.f, 7.f); t7.authoredWindow(32, 7); t7.run();
            check(t7.identical(), "the restored (32,7) push marches BIT-IDENTICALLY to the authored (32,7) window");
        }
    }

    // ── S4. OLD PATCHES AT ≤16 UNCHANGED — the restore path at the old domain ───
    {
        bool sweepOK = true;
        for (int L = 1; L <= 16; ++L) {
            for (int off : {0, 5, 15}) {
                SequencerEngine eng; eng.reset();
                eng.updateWindow((float)L, 0.f, false, (float)off, 0.f, false);
                const uint64_t want = SequencerEngine::windowMaskBits(L, off);
                sweepOK = sweepOK
                    && eng.cachedLength == L && eng.cachedOffset == off
                    && eng.windowMask == want;
                if (!sweepOK) {
                    std::printf("        old-patch (L=%d, off=%d) -> (%d,%d) mask want=%016llx got=%016llx\n",
                                L, off, eng.cachedLength, eng.cachedOffset,
                                (unsigned long long)want, (unsigned long long)eng.windowMask);
                    break;
                }
            }
            if (!sweepOK) break;
        }
        check(sweepOK, "old-patch lengths 1..16 x offsets {0,5,15} restore EXACTLY");
        // The old-patch WRAPPING configs march bit-identically restored vs authored:
        //   (12,10): crosses the bar line — at this ring the window EXTENDS
        //            (cells 10..21, the D3 documented change; GATE 1 owns the
        //            cross-flip equivalence, this owns the restore transparency);
        //   (16,5):  the full old ring shifted (cells 5..20).
        {
            TwinMarch tw(0xD1CE55ULL);
            tw.restoredPush(12.f, 10.f); tw.authoredWindow(12, 10); tw.run();
            check(tw.identical(), "the restored (12,10) old-patch window marches BIT-IDENTICALLY to authored (the bar-crossing extension)");
            TwinMarch tf(0xF00566ULL);
            tf.restoredPush(16.f, 5.f); tf.authoredWindow(16, 5); tf.run();
            check(tf.identical(), "the restored (16,5) old-patch window marches BIT-IDENTICALLY to authored (the full old ring, shifted)");
        }
    }

    std::printf("test_stage4_persistence: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
