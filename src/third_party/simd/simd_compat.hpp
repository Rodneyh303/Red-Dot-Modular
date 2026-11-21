#pragma once
// simd_compat.hpp — one include for SSE + AVX2 intrinsics across x86 and arm64.
//
// Uses the Rack SDK's bundled SIMDe. SIMDe provides the full SSE + AVX intrinsic set:
// native pass-through on x86 (with -msse4.1 / -mavx2), NEON emulation on arm64, so the
// SAME intrinsic source compiles and runs on both archs.
//
// SIMDE_ENABLE_NATIVE_ALIASES makes SIMDe expose the BARE names (__m128i, _mm_*,
// __m256d, _mm256_*), not just its simde-prefixed forms — so code written in plain
// intrinsics works unchanged.
//
// We do NOT vendor sse2neon — it conflicts with Rack's SIMDe (__m64 redefinition).
//
// Philox uses INTEGER SSE ops (mul/xor/add/shift/shuffle/blend), bit-exact across
// native-SSE and SIMDe-NEON -> the arm64 path is bit-identical.
// sumZ uses DOUBLE AVX2 ops (mul/add), 4-wide lane-independent + fixed h-sum ->
// bit-identical across native-AVX2, SIMDe-NEON, and the scalar 4-wide fallback.
//
// Usage: include this, then use _mm_* / _mm256_* intrinsics.
// Guard SSE kernels with #if defined(SIMD_HAVE_SSE).
// Guard AVX2 kernels with #if defined(SIMD_HAVE_AVX2) || defined(__aarch64__).

#ifndef SIMDE_ENABLE_NATIVE_ALIASES
#define SIMDE_ENABLE_NATIVE_ALIASES
#endif
#include <simde/x86/sse4.2.h>
#include <simde/x86/avx.h>        // __m256d 4-wide double (AVX1: mul/add/load/store) for sumZ
#define SIMD_HAVE_SSE 1
#if defined(__AVX__)
#define SIMD_HAVE_AVX 1           // native AVX1+ (4-wide double; Sandy Bridge 2011+)
#endif
