#pragma once
// simd_compat.hpp — one include for SSE intrinsics across x86 and arm64.
//
// On x86 (Windows/Linux/Intel-Mac slice): native SSE headers.
// On arm64 (Apple Silicon Mac slice, etc.): sse2neon translates the SSE
//   intrinsics (_mm_*) to ARM NEON at compile time, so the SAME SSE source
//   compiles and runs on ARM. sse2neon is header-only, MIT licensed
//   (src/third_party/sse2neon/LICENSE, (c) 2015-2026 SSE2NEON Contributors).
//
// Philox uses INTEGER SSE ops only (mul/xor/add/shift/shuffle/blend), which are
// bit-exact across SSE and NEON — so the arm64 (sse2neon) path is bit-identical
// to the x86 (native SSE) path. Verify via the bit-compare test.
//
// Usage: include this instead of <emmintrin.h>/<smmintrin.h> directly, then use
// _mm_* intrinsics normally. Guard the SIMD kernel with #if defined(SIMD_HAVE_SSE).

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // Native x86 SSE
    #include <immintrin.h>
    #define SIMD_HAVE_SSE 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    // ARM64 — translate SSE -> NEON via sse2neon (MIT, vendored)
    #include "sse2neon.h"
    #define SIMD_HAVE_SSE 1
    #define SIMD_VIA_SSE2NEON 1
#else
    // Unknown arch: no SSE path; callers fall back to scalar.
    #undef SIMD_HAVE_SSE
#endif
