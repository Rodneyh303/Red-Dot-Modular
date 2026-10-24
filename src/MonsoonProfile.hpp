// src/MonsoonProfile.hpp — CC_PROMPT_cpu_section_profile: diagnostic scaffolding for the
// clock-mode CPU hunt (Monsoon 8% / Macro 124%). Compile the plugin with
//     make RACK_DIR=../Rack-SDK EXTRA_FLAGS=-DMONSOON_PROFILE
// and NEVER ship that build — the flag-off path below compiles to NOTHING (two no-op
// macros), so normal builds are behaviour-identical. Remove this file together with the
// MONSOON_PROF_* call sites once the hot section is named.
//
// THE SAFE PATTERN (the earlier 124% was std::chrono called PER SAMPLE):
//   • now() ONLY AROUND SECTIONS (bracketing): MONSOON_PROF_BEGIN(acc) ... MONSOON_PROF_END(acc),
//     accumulating elapsed TICKS into a static accumulator the .cpp declares. On x86 now()
//     is __rdtsc (~10 cycles); elsewhere steady_clock ns.
//   • LOG ONLY at control rate (~every 50000 samples) — the .cpp log sites convert ticks→ns
//     via calibrateNsPerTick(), which samples chrono ONLY at those log events.
//   • Read RELATIVE SHARES, not absolutes: the dominant section is the fix target regardless
//     of the profiler's own bracketing overhead.
#pragma once

#ifdef MONSOON_PROFILE

#include <atomic>
#include <chrono>
#include <cstdint>

namespace monsoonProfile {

// Cheap monotonic counter — rdtsc on x86 (invariant TSC), steady_clock ns elsewhere
// (then calibrateNsPerTick() measures ~1.0 ns/tick and the maths still works).
// NOTE: deliberately __builtin_ia32_rdtsc() with NO <x86intrin.h> include — the full
// x86intrin family (AVX512/XOP/...) collides with SIMDe in TUs that include the SDK's
// SIMD compat layer (redefinition storms); the builtin needs no header at all.
inline uint64_t now() {
#if defined(__x86_64__) || defined(__i386__)
    return __builtin_ia32_rdtsc();
#else
    return (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
#endif
}

// FAST-PATH Suspect A: total call count of SequencerEngine::effectiveProbability (the
// ONE effective-probability accessor, CC_PROMPT_UI_TO_RESOLVER_SPEC — it subsumes the
// deleted a2ccb842 one-accessor design), incremented in the accessor itself so BOTH
// consumers are counted — the Macro prob-CV sweep (engine thread, STEP-cached by the
// spec's step-3 cache) and the bars' refresh (UI thread; hence the relaxed atomic).
inline std::atomic<uint64_t> effProbCalls{0};

// ticks→ns calibration, updated only at control-rate log events (chrono never runs per
// sample). Returns 0 until one log-event pair >=100 ms apart has been seen — log sites
// print "calibrating" for that first window. Shared by all log sites (one engine thread).
inline double calibrateNsPerTick(uint64_t tscNow) {
    static uint64_t prevTsc = 0;
    static std::chrono::steady_clock::time_point prevChrono{};
    static double nsPerTick = 0.0;
    const auto cNow = std::chrono::steady_clock::now();
    if (prevTsc && tscNow > prevTsc) {
        const double dTsc = (double)(tscNow - prevTsc);
        const double dNs = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(
                               cNow - prevChrono).count();
        if (dNs >= 1.0e8 && dTsc > 0.0)   // >=100 ms window -> stable estimate
            nsPerTick = dNs / dTsc;
    }
    prevTsc = tscNow;
    prevChrono = cNow;
    return nsPerTick;
}

} // namespace monsoonProfile

// Section brackets. The accumulator (monsoonProfAcc_<name>) is declared by the .cpp inside
// its own #ifdef MONSOON_PROFILE block; the t0 local is unique per section name.
#define MONSOON_PROF_BEGIN(acc) const uint64_t monsoonProfT0_##acc = monsoonProfile::now()
#define MONSOON_PROF_END(acc)   monsoonProfAcc_##acc += monsoonProfile::now() - monsoonProfT0_##acc

#else // !MONSOON_PROFILE — flag-off: NOTHING (byte-identical behaviour)

#define MONSOON_PROF_BEGIN(acc) ((void)0)
#define MONSOON_PROF_END(acc)   ((void)0)

#endif // MONSOON_PROFILE
